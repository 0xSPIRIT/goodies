#define CustomField(X) (custom_menu.options[X].field)
#define CustomCheckbox(X) (custom_menu.options[X].checkbox.active)
#define CustomColor(X) string_to_hex_rgb(CustomField(X).stable_input)

enum CustomOption {
    //DescriptionFirst,
    OpenIncognito,
    UseChrome,
    Font,
    FontSize,
    BackgroundColor,
    TextColor,
    AccentColor,
    TitleText, // If none, don't draw it
    GoodieFile,
    DeleteConfigFile,
    Count
};

enum OptionType {
    Checkbox,
    Textfield,
    Buttons,
};

struct CustomizeOptionType {
    CustomOption option;
    char name[64];
    OptionType type;
};

struct Text_Field {
    int id;
    SDL_Rect rect;
    bool focus;
    char input[MAX_STRING_SIZE];
    char stable_input[MAX_STRING_SIZE]; // Set upon the user clicking the field
};
Text_Field *editing_field = null;

struct Custom_Checkbox {
    int active;
    SDL_Rect rect;
};

struct Custom_Option {
    CustomizeOptionType type;
    Text_Field field;
    Custom_Checkbox checkbox;
    SDL_Rect visible_rect;
};

struct Custom_Menu {
    bool active;
    Custom_Option options[CustomOption::Count];
    int x, y;
};

Custom_Menu custom_menu = {};
bool update_custom_menu = false;

SDL_Color translate_color(Uint32 int_color) {
    SDL_Color result;
    result.r = (Uint8)((int_color & 0xFF0000) >> 16);
    result.g = (Uint8)((int_color & 0x00FF00) >> 8);
    result.b = (Uint8)((int_color & 0x0000FF) >> 0);
    result.a = 255;
    return result;
}

SDL_Color string_to_hex_rgb(char *string) {
    if (strlen(string) != 6) return SDL_Color{};
    SDL_Color out = {};
    
    Uint32 out_u32 = (Uint32)strtol(string, NULL, 16);
    
    return translate_color(out_u32);
}

void RunCheckbox(Custom_Checkbox *checkbox) {
    Button b = {};
    b.w = checkbox->rect.w;
    b.h = checkbox->rect.h;
    ButtonResult result = TickButton(&b, checkbox->rect.x, checkbox->rect.y);
    
    if (result.highlighted) {
        SDL_Color bg = CustomColor(CustomOption::BackgroundColor);
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    }
    
    SDL_RenderFillRect(renderer, &checkbox->rect);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &checkbox->rect);
    
    if (result.clicked) {
        checkbox->active = !checkbox->active;
    }
    
    if (checkbox->active) {
        SDL_Rect r = {
            1+checkbox->rect.x + checkbox->rect.w/4,
            1+checkbox->rect.y + checkbox->rect.h/4,
            checkbox->rect.w/2,
            checkbox->rect.h/2
        };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &r);
    }
}

void UpdateTextField(Text_Field *field) {
    if (!field) return;
    if (field->id == CustomOption::Font || field->id == CustomOption::FontSize) {
        global_should_update_text = true;
        TTF_Font *newfont = TTF_OpenFont(CustomField(CustomOption::Font).input, atoi(CustomField(CustomOption::FontSize).input));
        if (newfont && font) {
            TTF_CloseFont(font);
            font = newfont;
        }
        
        TTF_Font *new_titlefont = TTF_OpenFont(CustomField(CustomOption::Font).input, atoi(CustomField(CustomOption::FontSize).input)+18);
        if (title_font && new_titlefont) {
            TTF_CloseFont(title_font);
            title_font = new_titlefont;
        }
    }
        
    strcpy(field->stable_input, field->input);
}

void RunTextField(Text_Field *field) {
    bool hovering = PointIntersectsWithRect({mx, my}, field->rect);
    if (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        field->focus = hovering;
        strcpy(field->stable_input, field->input);
        if (field->focus) {
            editing_field = field;
            update_custom_menu = false;
        }
    } else {
        update_custom_menu = false;
    }
    
    int min_width = 125;
    
    if (field->input[0] == 0) {
        // Set dummy values for the w&h
        TTF_SizeText(font, "A", null, &field->rect.h);
        field->rect.w = min_width;
    }
    
    TextDrawData text_data = {};
    sprintf(text_data.id, "field %d", field->id);
    strcpy(text_data.string, field->input);
    text_data.x = field->rect.x;
    text_data.y = field->rect.y;
    text_data.font = font;
    text_data.wrapped = true;
    text_data.wrap_width = window_width-PAD*2;
    text_data.color = CustomColor(CustomOption::TextColor);
    
    {
        SDL_Color bg = CustomColor(CustomOption::BackgroundColor);
        if (hovering) {
            SDL_SetRenderDrawColor(renderer, 64, 64, 64, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        }
        SDL_RenderFillRect(renderer, &field->rect);
    }
    
    TextDraw(renderer, &text_data);
    
    int actual_width = text_data.w;
    
    if (field->input[0]) {
        field->rect.h = text_data.h;
        field->rect.w = max(min_width, text_data.w);
    }
    
    if (field->focus) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    } else {
        SDL_SetRenderDrawColor(renderer,
                               text_data.color.r,
                               text_data.color.g,
                               text_data.color.b,
                               255);
    }
    
    field->rect.h--;
    SDL_RenderDrawRect(renderer, &field->rect);
    
    if (field->focus) {
        int x = field->rect.x+actual_width;
        if (field->input[0] == 0) x = field->rect.x;
        DrawCursor(font, x, field->rect.y);
    }
}

void SetupCustomMenu(bool defaults) {
    CustomizeOptionType custom_option_key[] = {
        //{CustomOption::DescriptionFirst, "Description First",  OptionType::Checkbox},
        {CustomOption::OpenIncognito,    "Open Links Privately", OptionType::Checkbox},
        {CustomOption::UseChrome,        "Use Chrome",         OptionType::Checkbox},
        {CustomOption::Font,             "Font",               OptionType::Textfield},
        {CustomOption::FontSize,         "Font Size",          OptionType::Textfield},
        {CustomOption::BackgroundColor,  "Background Color",   OptionType::Textfield},
        {CustomOption::TextColor,        "Text Color",         OptionType::Textfield},
        {CustomOption::AccentColor,      "Accent Color",       OptionType::Textfield},
        {CustomOption::TitleText,        "Title Text",         OptionType::Textfield},
        {CustomOption::GoodieFile,       "Goodies File Path",  OptionType::Textfield},
        {CustomOption::DeleteConfigFile, "Delete Config File & Exit", OptionType::Buttons}
    };
    
    for (int i = 0; i < CustomOption::Count; i++) {
        custom_menu.options[i].type = custom_option_key[i];
        if (custom_option_key[i].type == Textfield) {
            custom_menu.options[i].field.id = i;
        }
    }
    
    if (defaults) {
        CustomCheckbox(CustomOption::OpenIncognito) = true;
        
        strcpy(CustomField(CustomOption::Font).input, "C:/Windows/Fonts/bookosi.ttf");
        strcpy(CustomField(CustomOption::FontSize).input, "22");
        
        strcpy(CustomField(CustomOption::BackgroundColor).input, "000000");
        strcpy(CustomField(CustomOption::TextColor).input, "FFFFFF");
        strcpy(CustomField(CustomOption::AccentColor).input, "a03c3c");
        strcpy(CustomField(CustomOption::TitleText).input, "Goodies");
        strcpy(CustomField(CustomOption::GoodieFile).input, filepath);
    }
    
    for (int i = 0; i < CustomOption::Count; i++) {
        Custom_Option *option = &custom_menu.options[i];
        if (option->type.type == OptionType::Textfield)
            strcpy(option->field.stable_input, option->field.input);
    }
}

void DrawCustomMenu() {
    if (!custom_menu.active) return;
    int cumh = 0;
    
    custom_menu.x = window_width/8;
    custom_menu.y = window_height/8;
    
    update_custom_menu = true;
    
    SDL_Rect r = {
        custom_menu.x,
        custom_menu.y,
        6*window_width/8,
        6*window_height/8
    };
    
    SDL_Color bg = CustomColor(CustomOption::BackgroundColor);
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(renderer, &r);
    
    SDL_Color accent = CustomColor(CustomOption::AccentColor);
    SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
    
    SDL_RenderDrawRect(renderer, &r);
    
    {
        int h;
        TTF_SizeText(font, "Customize", NULL, &h);
        
        TextDrawData text_data = {};
        sprintf(text_data.id, "custom title");
        strcpy(text_data.string, "Customize");
        text_data.x = r.x;
        text_data.y = r.y - h;
        text_data.font = font;
        text_data.color = CustomColor(CustomOption::TextColor);
        
        SDL_Rect text_rect = {
            text_data.x, text_data.y,
            0, 0,
        };
        TTF_SizeText(text_data.font, text_data.string, &text_rect.w, &text_rect.h);
        
        SDL_SetRenderDrawColor(renderer, accent.r, accent.g, accent.b, 255);
        SDL_RenderDrawRect(renderer, &text_rect);
        
        text_rect.x++;
        text_rect.y++;
        text_rect.w-=2;
        // leave h
        
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
        SDL_RenderFillRect(renderer, &text_rect);
        
        TextDraw(renderer, &text_data);
    }
    
    bool any_field_has_focus = false;
    
    for (int i = 0; i < CustomOption::Count; i++) {
        Custom_Option *option = &custom_menu.options[i];
        
        if (option->field.focus) any_field_has_focus = true;
        
        // Drawing the name of the option
        TextDrawData text_data = {};
        sprintf(text_data.id, "custom %d", i);
        strcpy(text_data.string, option->type.name);
        text_data.x = custom_menu.x+PAD/2;
        text_data.y = custom_menu.y+cumh+PAD/2;
        text_data.font = font;
        text_data.wrapped = true;
        text_data.wrap_width = window_width-PAD*2;
        text_data.color = CustomColor(CustomOption::TextColor);
        
        TextDraw(renderer, &text_data);
        
        int xoff = atoi(CustomField(CustomOption::FontSize).stable_input)*12;
        
        switch (option->type.type) {
            case OptionType::Textfield: {
                option->field.rect.x = custom_menu.x + xoff;
                option->field.rect.y = text_data.y;
                
                RunTextField(&option->field);
            } break;
            case OptionType::Checkbox: {
                option->checkbox.rect.x = custom_menu.x + xoff;
                option->checkbox.rect.y = text_data.y;
                TTF_SizeText(text_data.font, "A", NULL, &option->checkbox.rect.w);
                option->checkbox.rect.h = option->checkbox.rect.w;
                RunCheckbox(&option->checkbox);
            } break;
            case OptionType::Buttons: {
                int w, h;
                TTF_SizeText(font, text_data.string, &w, &h);
                
                SDL_Rect re = { text_data.x, text_data.y, w, h };
                
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &re);
                
                Button b = {};
                b.w = w;
                b.h = h;
                
                ButtonResult result = TickButton(&b, re.x, re.y);
                
                if (result.highlighted) {
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 64);
                    SDL_RenderFillRect(renderer, &re);
                }
                
                if (result.clicked) {
                    DeleteFileA(config_path);
                    SaveToFile();
                    //FreeEverything(); // There's no reason to free everything here. The OS cleans everything.
                    exit(0);
                }
            } break;
        }
        
        cumh += text_data.h;
    }
    
    if (!any_field_has_focus)
        editing_field = false;
    
    if (update_custom_menu)
        UpdateTextField(editing_field);
}

void WriteConfig(void) {
    assert(*config_path);
    
    FILE *fp = fopen(config_path, "w");
    
    for (int i = 0; i < CustomOption::Count; i++) {
        Custom_Option *option = &custom_menu.options[i];
        
        switch (option->type.type) {
            case OptionType::Checkbox: {
                fprintf(fp, "%d\n", option->checkbox.active);
            } break;
            case OptionType::Textfield: {
                if (*option->field.input == 0) {
                    fprintf(fp, "EMPTY-STRING\n");
                } else {
                    fprintf(fp, "%s\n", option->field.input);
                }
            } break;
        }
    }
    
    fclose(fp);
}

void LoadConfig(void) {
    assert(*config_path);
    
    FILE *fp = fopen(config_path, "r");
    
    assert(fp);
    
    unsigned long pos = ftell(fp);
    char c;
    fscanf(fp, "%c", &c);
    
    fseek(fp, pos, SEEK_SET);
    
    if (c < '0' || c > '9') {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                                 "Error!",
                                 "It seems like you're using a config file from an older version of goodies.\nI'll exit, and delete your old config file.\nWhen you reopen, you can reset the path to your goodies file as it was before!",
                                 window);
        if (window) FreeEverything();
        fclose(fp);
        DeleteFileA(config_path);
        exit(0);
    }
    
    for (int i = 0; i < CustomOption::Count; i++) {
        Custom_Option *option = &custom_menu.options[i];
        
        switch (option->type.type) {
            case OptionType::Checkbox: {
                fscanf(fp, "%d\n", &option->checkbox.active);
            } break;
            case OptionType::Textfield: {
                char str[MAX_STRING_SIZE] = {};
                fscanf(fp, "%[^\n]\n", str);
                
                if (strcmp(str, "EMPTY-STRING") == 0) break;
                
                strcpy(option->field.stable_input, str);
                strcpy(option->field.input, option->field.stable_input);
                if (option->type.option == CustomOption::GoodieFile) {
                    strcpy(filepath, option->field.stable_input);
                }
                
                if (option->type.option == CustomOption::GoodieFile ||
                    option->type.option == CustomOption::Font) {
                    
                    if (!FileExists(option->field.stable_input)) {
                        fclose(fp);
                        CommonFileErrorAndExit(option->field.stable_input, config_path);
                    }
                    
                }
            } break;
        }
    }
    
    fclose(fp);
}

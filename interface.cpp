struct Selection {
    SDL_Rect box;
    Link *links[1024];
    int link_count;
    
    bool isopen() {
        return box.w == box.h == 0;
    }
};
Selection selection = { {-1, -1, 0, 0} };

static void SetSelectionLinks(Link *start) {
    for (int i = 0; i < 1024; i++)
        selection.links[i] = null;
    selection.link_count = 0;
    
    SDL_Rect box = selection.box;
    
    if (box.w < 0) {
        box.w *= -1;
        box.x -= box.w;
    }
    if (box.h < 0) {
        box.h *= -1;
        box.y -= box.h;
    }
    
    for (Link *a = start;
         a;
         a = a->next)
    {
        a->highlighted = false;
        if (RectIntersectsWithRect(a->link_visible_rect, box) ||
            RectIntersectsWithRect(a->desc_visible_rect, box)) {
            if (selection.link_count < 1024) {
                selection.links[selection.link_count++] = a;
                a->highlighted = true;
            }
        }
        SetSelectionLinks(a->child);
    }
}

struct Button {
    SDL_Texture *texture;
    char text[MAX_STRING_SIZE]; // optional
    int w, h;
    bool highlighted;
};

struct ButtonResult {
    bool highlighted, clicked;
};

static ButtonResult TickButton(Button *button, int x, int y) {
    ButtonResult result = {};
    
    SDL_Point mouse = {mx, my};
    SDL_Rect rect = {x, y - (int)view_y, button->w, button->h};
    
    if (button->texture)
        SDL_SetTextureColorMod(button->texture, 255, 255, 255);
    
    if (PointIntersectsWithRect(mouse, rect)) {
        result.highlighted = true;
        if (button->texture)
            SDL_SetTextureColorMod(button->texture, 127, 127, 127);
        if (mouse_clicked)
            result.clicked = true;
    }

    if (button->texture)
        SDL_RenderCopy(renderer, button->texture, NULL, &rect);

    return result;
}

#define MAX_MENU_OPTIONS 10

enum MenuOp {
    OnLink,
    OutsideLink
};

struct MenuOperation {
    MenuOp op;
    Hover hover; // A struct that contains link and if it's editing link or description
};

struct MenuOption {
    char string[MAX_STRING_SIZE];
    void (*hook)(MenuOperation);
};

struct Menu {
    SDL_Texture *target;
    MenuOperation op;
    MenuOption options[MAX_MENU_OPTIONS];
    int option_count;
    int x, y;
    bool active;
};
Menu global_menu = {};

static void MenuOff(Menu *menu) {
    menu->active = false;
    state = State::NORMAL;
    mouse_clicked = false;
}

#define prf() printf("%s\n", __func__)

// Hooks
void menu_edit_link(MenuOperation operation) {
    global_menu.active = false;
    printf("%d, %s\n", operation.hover.what_editing, operation.hover.link->link);
    state = operation.hover.what_editing;
    editing_link = operation.hover.link;
    first_edit = true;
}

static Link *AddChildLink(Link *parent, const char *link, const char *description);

static void menu_add_child_link(MenuOperation operation) {
    Link *link = operation.hover.link;
    
    Link *n = AddChildLink(link, "", "");
    
    n->editing = true;
    editing_link = n;
    state = State::EDITING_NAME;
    global_menu.active = false;
}

static void menu_insert_link(MenuOperation operation) {
    Link *link = operation.hover.link;
    
    Link *n = null;
    n = (Link*)calloc(1, sizeof(Link));
    if (link->next) {
        Link *temp = link->next;
        link->next = n;
        link->next->next = temp;
    } else {
        link->next = n;
    }
    link->next->prev = link;
    link->next->parent = link->parent;
    link->next->editing = true;
    link->next->id = 2*link_count;
    editing_link = link->next;
    
    char *clipboard = SDL_GetClipboardText();
    if (clipboard){
        // clean the text
        char *s = clipboard;
        int i = 0;
        while (*s){
            if (*s == '\n' || *s == '\r' || *s == '\t') {
                ++s;
                continue;
            }
            editing_link->link[i++] = *s;
            ++s;
        }
        editing_link->link[i] = 0;
        
        SDL_free(clipboard);
    }
    
    state = State::EDITING_NAME;
    global_menu.active = false;
}

static void menu_add_link(MenuOperation operation) {
    (void)operation;
    Link *n = AddChildLink(null, "", "");
    n->editing = true;
    editing_link = n;
    state = State::EDITING_NAME;
    global_menu.active = false;
}

static void menu_edit_description(MenuOperation operation) {
    assert(operation.hover.link);
    state = State::EDITING_DESC;
    editing_link = operation.hover.link;
    global_menu.active = false;
}

static void menu_delete_link(MenuOperation operation) {
    Link *link = operation.hover.link;
    if (link->prev) {
        link->prev->next = link->next;
        if (link->next) link->next->prev = link->prev;
    } else if (link == start_link) {
        start_link = start_link->next;
        start_link->prev = null;
    } else {
        assert(link->parent);
        link->parent->child = link->next;
        if (link->next) link->next->prev = null;
    }
    free(link);
    global_menu.active = false;
    state = State::NORMAL;
}

static void SetMenuOptions(Menu *menu, MenuOption options[], int option_count) {
    menu->option_count = option_count;

    for (int i = 0; i < option_count; i++) {
        menu->options[i] = options[i];
    }
}

static Menu MakeMenu() {
    Menu menu = {};

    menu.target = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_UNKNOWN,
                                    SDL_TEXTUREACCESS_TARGET,
                                    1000,
                                    1000);
    assert(menu.target);
    SDL_SetTextureBlendMode(menu.target, SDL_BLENDMODE_BLEND);

    return menu;
}

static void FreeMenu(Menu *menu) {
    SDL_DestroyTexture(menu->target);
}

static void OpenMenu(Menu *menu, MenuOperation op) {
    menu->active = true;
    menu->x = mx;
    menu->y = my;
    menu->op = op;
    state = State::RIGHT_CLICK;

    switch(op.op){
        case MenuOp::OnLink: {
            if (op.hover.what_editing == State::EDITING_NAME) {
                MenuOption options[] = {
                    {"Edit", menu_edit_link},
                    {"Edit Description", menu_edit_description},
                    {"Add Child Link", menu_add_child_link},
                    {"Insert Link", menu_insert_link},
                    {"Delete Link", menu_delete_link}
                };
                SetMenuOptions(menu, options, 5);
            }
        } break;
        case MenuOp::OutsideLink: {
            MenuOption options[] = {
                {"Add Link", menu_add_link} };
            SetMenuOptions(menu, options, 1);
        } break;
    }
}

static void DrawMenu(Menu *menu) {
    if (!menu->active) return;

    assert(SDL_SetRenderTarget(renderer, menu->target) == 0);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    int cumh = 0;
    int width = 0;
    
    bool clicked_any_option = false;

    for (int i = 0; i < menu->option_count; i++) {
        TextDrawData data = {};
        sprintf(data.id, "menu %d", i);
        data.x = 0;
        data.y = cumh;
        strcpy(data.string, menu->options[i].string);
        data.wrapped = false;
        data.font = font;
        data.force_redraw = false;
        data.color = SDL_Color{255,255,255,255};
        int w, h;
        TTF_SizeText(data.font, data.string, &w, &h);
        
        Button b = {};
        b.texture = null;
        b.w = w;
        b.h = h;
        
        ButtonResult r = TickButton(&b, menu->x+data.x, menu->y+data.y);
        if (r.highlighted) {
            data.color = SDL_Color{127,127,127,255};
        }
        
        TextDraw(renderer, &data);
        cumh += data.h;
        
        if (r.clicked) {
            clicked_any_option = true;
            if (menu->options[i].hook) {
                menu->options[i].hook(menu->op);
            }
        }
        
        if (data.w > width) width = data.w;
    }
    
    if (!clicked_any_option && mouse_clicked) {
        MenuOff(menu);
    }

    SDL_SetRenderTarget(renderer, NULL);

    SDL_Rect r = {
        menu->x, menu->y,
        width, cumh
    };

    SDL_SetRenderDrawColor(renderer, 16, 16, 16, 255);
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &r);

    SDL_Rect dst = {
        menu->x, menu->y,
        1000, 1000
    };
    SDL_RenderCopy(renderer, menu->target, NULL, &dst);
}

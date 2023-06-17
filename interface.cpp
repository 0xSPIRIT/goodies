struct Button {
    SDL_Texture *texture;
    char text[MAX_STRING_SIZE]; // optional
    int w, h;
    bool highlighted;
};

bool TickButton(Button *button, int x, int y) {
    bool result = false;
    
    SDL_Point mouse = {mx, my};
    SDL_Rect rect = {x, y - (int)view_y, button->w, button->h};
    
    SDL_SetTextureColorMod(button->texture, 255, 255, 255);
    
    if (PointIntersectsWithRect(mouse, rect)) {
        SDL_SetTextureColorMod(button->texture, 127, 127, 127);
        if (mouse_clicked)
            result = true;
    }
    
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

struct Menu {
    SDL_Texture *target;
    MenuOperation op;
    char options[MAX_MENU_OPTIONS][MAX_STRING_SIZE];
    int option_count;
    int x, y;
    bool active;
};

static void SetMenuOptions(Menu *menu, char *options[], int option_count) {
    menu->option_count = option_count;
    
    for (int i = 0; i < option_count; i++) {
        strcpy(menu->options[i], options[i]);
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
                char *options[] = { "Edit", "Add Child Link", "Insert Link" };
                SetMenuOptions(menu, options, 3);
            }
        } break;
        case MenuOp::OutsideLink: {
            char *options[] = { "Add Link" };
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
    
    for (int i = 0; i < menu->option_count; i++) {
        TextDrawData data = {};
        sprintf(data.id, "menu %d", i);
        data.x = 0;
        data.y = cumh;
        strcpy(data.string, menu->options[i]);
        data.wrapped = false;
        data.font = font;
        data.force_redraw = false;
        data.color = SDL_Color{255,255,255,255};
        TextDraw(renderer, &data);
        cumh += data.h;
        
        if (data.w > width) width = data.w;
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
struct Selection {
    bool active;
    int stored_x, stored_y;
    Link *holding_link;
    SDL_Rect box;
    Link *links[1024];
    int link_count;
};
static Selection selection = {false, -1, -1, null, {-1, -1, -1, -1}, null, 0 };

static void ClearSelections(void) {
    for (int i = 0; i < 1024; i++) {
        if (selection.links[i] == null) continue;
        selection.links[i]->selected = false;
        selection.links[i] = null;
    }
    selection.link_count = 0;
}

static void SetSelectionLinks(Link *start) {
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
        a->selected = false;
        if (RectIntersectsWithRect(a->link_visible_rect, box) ||
            RectIntersectsWithRect(a->desc_visible_rect, box)) {
            if (selection.link_count < 1024) {
                selection.links[selection.link_count++] = a;
                a->selected = true;
            }
        }
        
        SetSelectionLinks(a->child);
    }
}

void SelectAll(Link *start) {
    for (Link *l = start; l; l = l->next) {
        l->selected = true;
        selection.links[selection.link_count++] = l;
        SelectAll(l->child);
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

    SDL_Point ms = {mx, my};
    SDL_Rect rect = {x, y - (int)view_y, button->w, button->h};

    if (PointIntersectsWithRect(ms, rect)) {
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

void menu_copy_link(MenuOperation operation) {
    global_menu.active = false;
    state = State::NORMAL;
    SDL_SetClipboardText(operation.hover.link->link);
}

void menu_edit_link(MenuOperation operation) {
    global_menu.active = false;
    state = operation.hover.what_editing;
    editing_link = operation.hover.link;
    editing_link->editing = true;
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

static void menu_insert_link(MenuOperation operation, bool after) {
    Link *link = operation.hover.link;

    if (after) {
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
        link->next->id = link_count++;
        editing_link = link->next;
    } else {
        Link *n = null;
        n = (Link*)calloc(1, sizeof(Link));
        if (link->prev) {
            Link *old_next = link->prev->next;
            link->prev->next = n;
            n->next = old_next;
            link->prev = n;
        } else if (link->parent) {
            n->next = link;
            n->parent = link->parent;
            link->parent->child = n;
            link->prev = n;
        } else {
            n->next = start_link;
            start_link->prev = n;
            start_link = n;
        }
        n->editing = true;
        editing_link = n;
    }
    state = State::EDITING_NAME;
    global_menu.active = false;
}

static void menu_insert_link_after(MenuOperation operation) {
    menu_insert_link(operation, true);
}

static void menu_insert_link_before(MenuOperation operation) {
    menu_insert_link(operation, false);
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
        if (start_link)
            start_link->prev = null;
    } else {
        assert(link->parent);
        link->parent->child = link->next;
        if (link->next) link->next->prev = null;
    }
    FreeLinks(link->child);
    free(link);
    global_menu.active = false;
    state = State::NORMAL;
}

static void menu_toggle_collapse_link(MenuOperation operation) {
    Link *link = operation.hover.link;
    
    link->collapsed = !link->collapsed;
    
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
                    {"Copy", menu_copy_link},
                    {"Edit", menu_edit_link},
                    {"Edit Description", menu_edit_description},
                    {"Add Child Link", menu_add_child_link},
                    {"Insert Link After", menu_insert_link_after},
                    {"Insert Link Before", menu_insert_link_before},
                    {"Delete Link", menu_delete_link},
                    {"Toggle Collapse", menu_toggle_collapse_link}
                };
                SetMenuOptions(menu, options, 8);
            }
        } break;
        case MenuOp::OutsideLink: {
            MenuOption options[] = {
                {"Add Link To End", menu_add_link} };
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

        ButtonResult r = TickButton(&b, menu->x+data.x, menu->y+data.y+(int)view_y);
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

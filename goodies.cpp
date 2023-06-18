// TODO:
//  - Customization options (popup menu)
//      - Description first, then link.
//      - Font & Size
//      - Color
//      - Browser
//      - Load different goodie file
//      - Delete config file

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>

#include <shlobj_core.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define null NULL

#include "util.cpp"

#define PAD 32

struct Link {
    int id;
    char link[MAX_STRING_SIZE];
    char description[MAX_STRING_SIZE];
    bool highlighted, selected;
    bool editing; // Are we currently editing?
    SDL_Rect link_visible_rect,
    desc_visible_rect; // Gets set upon drawing.
    Link *next; // Next link at the same level
    Link *prev; // Previous link at the same level
    Link *child; // Next child link.
    Link *parent; // Parent link
};

enum State {
    NORMAL, // Normal shit
    EDITING_NAME,
    EDITING_DESC,
    RIGHT_CLICK
};

enum Browser {
    CHROME,
    FIREFOX
};

#define BrowserString(X) (X == CHROME ? "chrome" : "firefox")
Browser browser = FIREFOX;

static int state = State::NORMAL;
static bool first_edit = false;

static char config_path[MAX_PATH]={};

static Link *start_link = null;
static Link *curr_link = null; // The top-level thing.
static Link *editing_link = null;

struct Hover {
    Link *link;
    State what_editing; // State::EDITING_NAME or State::EDITING_DESC
};
static Hover hover = {};

static int link_count = 0;

static char filepath[MAX_PATH] = {};

static float view_y = 0, view_to_y = 0;
static float scroll_speed = 60;
static float scroll_t = 0.25f; // The t value used in the scroll lerp

static bool typed_this_frame = false;

static SDL_Window *window = null;
static SDL_Renderer *renderer = null;
static SDL_Texture *plus = null;
static int plus_w, plus_h;

static int mx, my;

static int window_width = 960, window_height = 960;
static bool mouse_clicked = false;
static const Uint8 *keys = null;

static TTF_Font *font = null;

static void FreeLinks(Link *start);

#include "interface.cpp"

static void FreeLinks(Link *start) {
    if (!start) return;

    Link *next = null;
    for (Link *a = start;
         a;
         a = next)
    {
        next = a->next;
        FreeLinks(a->child);
        free(a);
    }
}

static void OpenLinks(Link *links[], int lc) {
    char message[MAX_STRING_SIZE*2] = {};
    for (int i = 0; i < lc; i++)
        sprintf(message, "%s %s", message, links[i]->link);

    char command[MAX_STRING_SIZE] = {};
    if (browser == CHROME)
        sprintf(command, "%s -incognito", message);
    else if (browser == FIREFOX)
        sprintf(command, "-private-window \"%s\"", message);

    ShellExecuteA(0, 0, BrowserString(browser), command, 0, SW_SHOWMAXIMIZED);
}

static Link *AddChildLink(Link *parent, const char *link, const char *description) {
    Link *new_link = null;

    if (parent == null) { // Add to the end of the main list
        if (!start_link) {
            start_link = (Link*)calloc(1, sizeof(Link));
            curr_link = start_link;
            new_link = curr_link;
        } else {
            curr_link->next = (Link*)calloc(1, sizeof(Link));
            curr_link->next->prev = curr_link;
            curr_link = curr_link->next;
            new_link = curr_link;
        }
    } else {
        if (parent->child) {
            for (new_link = parent->child;
                 new_link->next;
                 new_link = new_link->next);
            new_link->next = (Link*)calloc(1, sizeof(Link));
            new_link->next->prev = new_link;
            new_link->next->parent = parent;
            new_link = new_link->next;
        } else {
            new_link = (Link*)calloc(1, sizeof(Link));
            new_link->parent = parent;
            parent->child = new_link;
        }
    }

    new_link->id = link_count++;
    strcpy(new_link->link, link);
    strcpy(new_link->description, description);


    return new_link;
}

// Returns the width of the cursor drawn
static int DrawCursor(TTF_Font *fnt, int x, int y) {
    static int width = -1;
    static int height = -1;

    if (height == -1 || width == -1) {
        TTF_SizeText(fnt, "|", &width, &height);
    }
    width = 2;

    SDL_Rect r = {
        x, y,
        width, height
    };

    double t = SDL_GetTicks()/1000.0;
    if (typed_this_frame || t - (int)t < 0.5) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    } else {
        return width;
    }
    SDL_RenderFillRect(renderer, &r);

    return width;
}

static bool LinkOutOfView(int x, int y) {
    (void)x;
    if (y > view_y+window_height) return true;
    if (y < view_y) return true;
    return false;
}

static int DrawLink(Link *link, int x, int y) {
    int link_w = 0;
    int height = 0;

    int end_x = 0;

    bool was_link_wrapped = false;

    // Move view
    if (link->editing && LinkOutOfView(x, y)) {
        view_to_y = (float)(y-window_height/2);
    }

    if (*link->link || link->editing) { // Link
        TextDrawData data = {};
        sprintf(data.id, "link %d", link->id);
        strcpy(data.string, link->link);
        data.x = x;
        data.y = y - (int)view_y;
        data.wrapped = true;
        data.font = font;
        data.force_redraw = false;
        data.wrap_width = window_width-PAD*2;
        data.color = SDL_Color{255, 255, 255, 255};

        TextDraw(renderer, &data);

        link->link_visible_rect = SDL_Rect{
            data.x, data.y,
            data.w, data.h
        };

        if (state == EDITING_NAME && editing_link == link) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &link->link_visible_rect);

            int cx = data.x+data.w;
            if (!*link->link) cx -= data.w;
            DrawCursor(font, cx, data.y);
        }

        link_w += data.w + 16;
        height = data.h;

        if (data.w >= window_width-PAD*4) {
            was_link_wrapped = true;
        }

        end_x = data.x + data.w + 16;
    }

    { // Description
        TextDrawData data = {};
        sprintf(data.id, "desc %d", link->id);
        strcpy(data.string, link->description);
        if (was_link_wrapped) {
            data.x = x;
            data.y = y - (int)view_y + height;
        } else {
            data.x = x + link_w;
            data.y = y - (int)view_y;
        }
        data.font = font;
        data.force_redraw = false;
        data.wrap_width = window_width - PAD - link_w;
        data.color = SDL_Color{160, 60, 60, 255};

        TextDraw(renderer, &data);

        link->desc_visible_rect = SDL_Rect{
            data.x, data.y,
            data.w, data.h
        };

        if (state == EDITING_DESC && editing_link == link) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawRect(renderer, &link->desc_visible_rect);

            int cx = data.x+data.w;
            if (!*link->description) cx -= data.w;
            DrawCursor(font, cx, data.y);
        }

        end_x = data.x + data.w + 16;

        if (!was_link_wrapped) {
            if (data.h > height) height = data.h;
        } else {
            height = height + data.h;
        }
    }

    if (link->next == null) {
        Button b = {};
        b.texture = plus;
        b.w = plus_w;
        b.h = plus_h;
        if (TickButton(&b, end_x, y-plus_h/2+height/2).clicked) {
            MenuOperation operation = {};
            operation.op = OnLink;
            operation.hover.link = link;
            menu_insert_link_after(operation);
        }
    }

    if (hover.link == link && link->highlighted) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        if (hover.what_editing == State::EDITING_NAME)
            SDL_RenderDrawRect(renderer, &link->link_visible_rect);
        else if (hover.what_editing == State::EDITING_DESC)
            SDL_RenderDrawRect(renderer, &link->desc_visible_rect);
    } else if (link->highlighted) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderDrawRect(renderer, &link->link_visible_rect);
    }

    return height;
}

static void SetAllLinksNotHighlighted(Link *start) {
    if (!start) return;

    for (Link *a = start;
         a;
         a = a->next)
    {
        a->highlighted = false;
        SetAllLinksNotHighlighted(a->child);
    }
}

// Find link that you're highlighted on and return it.
static Hover FindHoveredLink(Link *start) {
    if (!start) return {};

    for (Link *a = start;
         a;
         a = a->next)
    {
        if (PointIntersectsWithRect({mx, my}, a->link_visible_rect)) {
            return {a, State::EDITING_NAME};
        }
        if (PointIntersectsWithRect({mx, my}, a->desc_visible_rect)) {
            return {a, State::EDITING_DESC};
        }
        Hover result = FindHoveredLink(a->child);
        if (result.link) return result;
    }

    return {};
}

static int DrawLinkAndChildLinks(Link *link, int x, int y) {
    if (!link) return 0;

    int cumh = 0;
    for (Link *a = link;
         a;
         a = a->next)
    {
        cumh += DrawLink(a, x, y+cumh);
        cumh += 8+DrawLinkAndChildLinks(a->child, 32+x, y+cumh+8);
    }

    return cumh;
}

static inline char *GetBufferFromState(int s) {
    char *buffer = null;
    if (s == State::EDITING_NAME) {
        assert(editing_link);
        buffer = editing_link->link;
    } else if (s == State::EDITING_DESC) {
        assert(editing_link);
        buffer = editing_link->description;
    }
    return buffer;
}

static void WriteLinksToFile(FILE *fp, Link *start, int layer) {
    if (!start) return;

    for (Link *a = start;
         a;
         a = a->next)
    {
        if (*a->link || *a->description) {
            fprintf(fp, "%d:%s|%s\n", layer, a->link, a->description);
        }
        WriteLinksToFile(fp, a->child, layer+1);
    }
}

static void SaveToFile() {
    FILE *fp = fopen(filepath, "w");
    WriteLinksToFile(fp, start_link, 0);
    fclose(fp);
}

static void LoadFile(const char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        char message[MAX_STRING_SIZE] = {};
        sprintf(message, "Couldn't load file %s\nExiting...", file);
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error!", message, window);
        DeleteFile(config_path);
        exit(1);
    }

    #define MAX_CHILD_LINKS 128
    Link *prevlinks[MAX_CHILD_LINKS] = {};

    while (!feof(fp)) {
        int layer;

        char link_and_desc[MAX_STRING_SIZE]={};

        unsigned long pos = ftell(fp);
        char c;
        fscanf(fp, "%c", &c);

        fseek(fp, pos, SEEK_SET);

        if (c >= '0' && c <= '9') {
            fscanf(fp, "%d:%[^\n]\n", &layer, link_and_desc);
        } else { // Compatibility with raw files with links not in our format
            fscanf(fp, "%[^\n]\n", link_and_desc);
            layer=0;
        }

        size_t last = strlen(link_and_desc)-1;
        if (link_and_desc[last] == '\n')
            link_and_desc[last] = 0;

        if (link_and_desc[0] == 0) continue;

        // Determine between the link itself and description

        size_t len = strlen(link_and_desc);
        int pipe = -1;
        for (int i = 0; i < len; i++) {
            if (link_and_desc[i] == '|') {
                pipe = i;
                break;
            }
        }

        if (pipe == -1) { // Compatibility with raw files with links not in our format
            AddChildLink(null, link_and_desc, "");
        } else {
            char link[MAX_STRING_SIZE] = {};
            char *desc;
            strncpy(link, link_and_desc, pipe);
            desc = link_and_desc+pipe+1;

            Link *a = null;
            if (layer == 0) {
                a = AddChildLink(null, link, desc);
            } else {
                a = AddChildLink(prevlinks[layer-1], link, desc);
            }
            prevlinks[layer] = a;
        }
    }

    fclose(fp);
}

int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    char cwd[MAX_PATH]={};
    GetCurrentDirectory(MAX_PATH, cwd);

    SHGetFolderPathA(null, CSIDL_APPDATA, null, 0, config_path);
    sprintf(config_path, "%s\\goodies.cfg", config_path);
    FILE *fa = fopen(config_path, "r");

    if (!fa) {
        NewFile((char*)filepath);
        fa = fopen(config_path, "w");

        fprintf(fa, "%s\n", filepath);
        // TODO: Figure out if they use firefox or chrome
        int result = MessageBox(null, "Do you use chrome?\nOnly chrome and firefox are supported.", "Chrome?", MB_YESNO);
        if (result == IDYES) {
            browser = CHROME;
        } else if (result == IDNO) {
            browser = FIREFOX;
        } else {
            fclose(fa);
            DeleteFileA(config_path);
            exit(1);
        }
        fprintf(fa, "%d", browser == CHROME);
    } else {
        fscanf(fa, "%s\n", filepath);
        int use_chrome = false;
        fscanf(fa, "%d", &use_chrome);
        if (use_chrome) {
            browser = CHROME;
        } else {
            browser = FIREFOX;
        }
    }
    fclose(fa);

    SetCurrentDirectory(cwd); // Windows is horrible and resets our CWD

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("Goodies Viewer",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              window_width,
                              window_height,
                              SDL_WINDOW_RESIZABLE);
    assert(window);

    renderer = SDL_CreateRenderer(window,
                                  -1,
                                  SDL_RENDERER_PRESENTVSYNC);
    assert(renderer);

    global_menu = MakeMenu();

    SDL_Surface *surf = SDL_LoadBMP("plus.bmp");
    plus = SDL_CreateTextureFromSurface(renderer, surf);
    plus_w = surf->w;
    plus_h = surf->h;
    SDL_FreeSurface(surf);

    LoadFile(filepath);

    font = TTF_OpenFont("C:/Windows/Fonts/bookosi.ttf", 22);
    assert(font);

    TTF_Font *title_font = TTF_OpenFont("C:/Windows/Fonts/bookosi.ttf", 40);
    assert(title_font);

    bool running = true;

    SDL_Event event = {};
    bool has_event_queued = false;
    while (running) {
        mouse_clicked = false;
        typed_this_frame = false;

        if (has_event_queued)
            goto eventloop;

        while (SDL_PollEvent(&event)) {
            eventloop:
            switch (event.type) {
                case SDL_QUIT: { running = false; } break;
                case SDL_TEXTINPUT: {
                    typed_this_frame = true;
                    if (state == State::EDITING_NAME) {
                        assert(editing_link);
                        if (first_edit) {
                            memset(editing_link->link, 0, MAX_STRING_SIZE);
                            first_edit = false;
                        }
                        strcat(editing_link->link, event.text.text);
                    } else if (state == State::EDITING_DESC) {
                        assert(editing_link);
                        strcat(editing_link->description, event.text.text);
                    }
                } break;
                case SDL_KEYDOWN: {
                    typed_this_frame = true;
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: {
                            state = State::NORMAL;
                            if (editing_link && !*editing_link->link && !*editing_link->description) {
                                MenuOperation dummy_operation = {
                                    OnLink,
                                    {editing_link, State::NORMAL} // State doesn't matter for this op
                                };
                                menu_delete_link(dummy_operation);
                            }
                            editing_link = null;
                            global_menu.active = false;
                        } break;
                        case SDLK_RETURN: case SDLK_TAB: {
                            if (state == State::EDITING_NAME) {
                                state = State::EDITING_DESC;
                            } else if (state == State::EDITING_DESC) {
                                state = State::NORMAL;
                                editing_link->editing = false;
                                editing_link = null;
                            }
                        } break;
                        case SDLK_BACKSPACE: {
                            if (state != State::NORMAL) {
                                char *buffer = GetBufferFromState(state);
                                if (*buffer) {
                                    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) {
                                        memset(buffer, 0, strlen(buffer));
                                    }
                                    buffer[strlen(buffer)-1]=0;
                                }
                            }
                        } break;
                        case SDLK_v: {
                            char *buffer = GetBufferFromState(state);
                            if ((state == State::EDITING_NAME || state == State::EDITING_DESC) &&
                                (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]))
                            {
                                char *clipboard = SDL_GetClipboardText();
                                if (clipboard) {
                                    char *s = clipboard;
                                    size_t count = 0;
                                    while (*s) {
                                        if (*s != '\r' && *s != '\t' && *s != '\n' && count<MAX_STRING_SIZE) {
                                            *buffer++ = *s;
                                            ++count;
                                        }
                                        ++s;
                                    }
                                    *buffer = 0;
                                    SDL_free(clipboard);
                                }
                            }
                        } break;
                        case SDLK_s: {
                            if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL])
                                SaveToFile();
                        } break;
                    }
                } break;
                case SDL_WINDOWEVENT: {
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        window_width = event.window.data1;
                        window_height = event.window.data2;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    if (state == State::NORMAL && selection.box.x == -1) {
                        selection.box.x = mx;
                        selection.box.y = my+(int)view_y;
                        selection.stored_x = selection.box.x;
                        selection.stored_y = selection.box.y;
                    }
                } break;
                case SDL_MOUSEBUTTONUP: {
                    int button = event.button.button;

                    if (button == SDL_BUTTON_LEFT)
                        mouse_clicked = true;

                    if (!selection.isopen() && state == State::NORMAL) {
                        if (button == SDL_BUTTON_LEFT && hover.link) {
                            if (hover.what_editing == State::EDITING_DESC) {
                                state = hover.what_editing;
                                editing_link = hover.link;
                            } else {
                                Link *links[] = {hover.link};
                                OpenLinks(links, 1);
                            }
                        }
                        if (button == SDL_BUTTON_RIGHT) {
                            MenuOperation op;
                            if (hover.link) {
                                op.op = OnLink;
                                op.hover = hover;
                                op.hover.what_editing = State::EDITING_NAME;
                                // Force it to always be editing name because
                                // we kinda just want it to say the same thing anyways
                            } else {
                                op.op = OutsideLink;
                                op.hover = {};
                            }
                            OpenMenu(&global_menu, op);
                        }
                    } else if (state == State::RIGHT_CLICK) {
                        if(button==SDL_BUTTON_RIGHT) {
                            state = State::NORMAL;
                            global_menu.active = false;
                        }
                    } else if (state == State::EDITING_NAME || state == State::EDITING_DESC) {
                        state = State::NORMAL;
                        editing_link = null;
                    }
                } break;
                case SDL_MOUSEWHEEL: {
                    if (state == State::NORMAL)
                        view_to_y -= scroll_speed * event.wheel.y;
                } break;
            }
        }

        Uint32 mouse = SDL_GetMouseState(&mx, &my);
        keys = SDL_GetKeyboardState(0);

        view_y = lerp(view_y, view_to_y, scroll_t);

        if (selection.box.x != -1) {
            selection.box.x = selection.stored_x;
            selection.box.y = selection.stored_y - (int)view_y;
            selection.box.w = mx - selection.box.x;
            selection.box.h = my - selection.box.y;

            SetSelectionLinks(start_link);

            if (!(mouse & SDL_BUTTON_LEFT)) {
                // Execute some action.
                selection.box.x = selection.box.y = -1;
                selection.box.w = selection.box.h = 0;
                //SetAllLinksNotHighlighted(start_link);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        TextDrawData data = {};
        strcpy(data.id, "title");
        strcpy(data.string, "Goodies");
        data.x = PAD;
        data.y = PAD - (int)view_y;
        data.font = title_font;
        data.force_redraw = false;
        data.wrap_width = window_width - PAD*2;
        data.color = SDL_Color{200, 40, 20, 255};

        TextDraw(renderer, &data);

        DrawLinkAndChildLinks(start_link, PAD, 2*PAD+data.h);

        SetAllLinksNotHighlighted(start_link);
        if (state == State::NORMAL) {
            hover = FindHoveredLink(start_link);
            if (hover.link) {
                hover.link->highlighted = true;
            }
        }

        if (selection.isopen()) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 70, 120, 200, 50);
            SDL_RenderFillRect(renderer, &selection.box);
            SDL_SetRenderDrawColor(renderer, 70, 120, 250, 255);
            SDL_RenderDrawRect(renderer, &selection.box);
        }

        DrawMenu(&global_menu);

        SDL_RenderPresent(renderer);

        Uint32 flag = SDL_GetWindowFlags(window);
        if (!(flag & SDL_WINDOW_INPUT_FOCUS)) {
            SDL_WaitEvent(&event);
            has_event_queued = true;
        }
    }

    SaveToFile();

    for (int i = 0; i < text_cache_count; i++) {
        if (text_cache[i].texture)
            SDL_DestroyTexture(text_cache[i].texture);
    }
    SDL_DestroyTexture(plus);

    FreeMenu(&global_menu);
    FreeLinks(start_link);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    SDL_Quit();
    TTF_Quit();

    return 0;
}

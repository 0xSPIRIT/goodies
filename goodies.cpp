// TODO:
//  - When something is collapsed, select all child links as well
//  - Bulk actions with selections.
//    - Moving links under another link via click and drag
//  - Profile it to ensure when adding links, all the ID's are
//    proper and nothing has the IDs, making it re-draw text
//    every frame.

#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>

#include <windows.h>
#include <shlobj_core.h>
#include <stdio.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#define null 0

#include "util.cpp"

#define PAD 32



     // Taken from https://github.com/kumar8600/win32_SetProcessDpiAware/blob/master/win32_SetProcessDpiAware.c
     //
     // Created by kumar on 2016/03/29.
     //

     typedef enum PROCESS_DPI_AWARENESS
     {
         PROCESS_DPI_UNAWARE = 0,
         PROCESS_SYSTEM_DPI_AWARE = 1,
         PROCESS_PER_MONITOR_DPI_AWARE = 2
     } PROCESS_DPI_AWARENESS;

     typedef BOOL (WINAPI * SETPROCESSDPIAWARE_T)(void);
     typedef HRESULT (WINAPI * SETPROCESSDPIAWARENESS_T)(PROCESS_DPI_AWARENESS);

     inline bool win32_SetProcessDpiAware(void) {
         HMODULE shcore = LoadLibraryA("Shcore.dll");
         SETPROCESSDPIAWARENESS_T SetProcessDpiAwareness = NULL;
         if (shcore) {
             SetProcessDpiAwareness = (SETPROCESSDPIAWARENESS_T) GetProcAddress(shcore, "SetProcessDpiAwareness");
         }
         HMODULE user32 = LoadLibraryA("User32.dll");
         SETPROCESSDPIAWARE_T SetProcessDPIAware = NULL;
         if (user32) {
             SetProcessDPIAware = (SETPROCESSDPIAWARE_T) GetProcAddress(user32, "SetProcessDPIAware");
         }
    
         bool ret = false;
         if (SetProcessDpiAwareness) {
             ret = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE) == S_OK;
         } else if (SetProcessDPIAware) {
             ret = SetProcessDPIAware() != 0;
         }
    
         if (user32) {
             FreeLibrary(user32);
         }
         if (shcore) {
             FreeLibrary(shcore);
         }
         return ret;
     }


struct Link {
    int id;
    char link[MAX_STRING_SIZE];
    char description[MAX_STRING_SIZE];
    bool highlighted, selected;
    bool editing; // Are we currently editing?
    bool collapsed; // If true, it doesn't render child nodes.
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
    RIGHT_CLICK,
    CUSTOM_MENU
};

static int state = State::NORMAL;
static bool first_edit = false;

static char config_path[MAX_PATH];

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
static char text_input_this_frame = 0;

static SDL_Window *window = null;
static SDL_Renderer *renderer = null;
static SDL_Texture *plus = null, *arrow = null;
static int plus_w, plus_h, arrow_w, arrow_h;

static int mx, my, pmx, pmy;

static int window_width = 960;
static int window_height = 960;
static bool mouse_clicked = false;
static Uint32 mouse = 0;
static const Uint8 *keys = null;

static TTF_Font *font = null;
static TTF_Font *title_font = null;

static void FreeLinks(Link *start);
static void SaveToFile();
static int DrawCursor(TTF_Font *fnt, int x, int y);
static void FreeEverything();

#include "interface.cpp"
#include "customize.cpp"

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
    
    if (CustomCheckbox(CustomOption::OpenIncognito)) {
        if (CustomCheckbox(CustomOption::UseChrome)) {
            sprintf(command, "%s -incognito", message);
        } else {
            sprintf(command, "-private-window \"%s\"", message);
        }
    } else {
        strcpy(command, message);
    }
    
    const char *browser_string;
    
    if (CustomCheckbox(CustomOption::UseChrome)) {
        browser_string = "chrome";
    } else {
        browser_string = "firefox";
    }
    
    ShellExecuteA(0, 0, browser_string, command, 0, SW_SHOWMAXIMIZED);
}

static void OpenSelectedLinksNoChildren(Link *start) {
    Link **links = (Link**)calloc(link_count, sizeof(Link*));
    int i = 0;
        
    for (Link *a = start;
         a;
         a = a->next)
    {
        if (a->selected) {
            links[i++] = a;
        }
    }
    
    if (i)
        OpenLinks(links, i);
    
    free(links);
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
    int width = -1;
    int height = -1;

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

static void DrawLinkArrow(Link *link, int x, int y, int height) {
    if (!link->child) return;
    
    SDL_Rect dst = {
        x-(arrow_w*3)/2,
        y + height/2 - arrow_h/2 - (int)view_y,
        arrow_w,
        arrow_h
    };
    
    Button b = {};
    b.w = dst.w;
    b.h = dst.h;
    
    ButtonResult result = TickButton(&b, dst.x, dst.y + (int)view_y);
    
    if (result.highlighted) {
        SDL_SetTextureColorMod(arrow, 127, 127, 127);
    } else {
        SDL_Color col = CustomColor(CustomOption::AccentColor);
        SDL_SetTextureColorMod(arrow, col.r, col.g, col.b);
    }
    
    if (result.clicked) {
        link->collapsed = !link->collapsed;
    }
    
    if (!link->collapsed) {
        SDL_RenderCopyEx(renderer,
                         arrow,
                         NULL,
                         &dst,
                         90,
                         NULL,
                         SDL_FLIP_NONE);
    } else {
        SDL_RenderCopy(renderer, arrow, NULL, &dst);
    }
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
        data.color = CustomColor(CustomOption::TextColor);

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
        data.color = CustomColor(CustomOption::AccentColor);

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
    
    DrawLinkArrow(link, x, y, height);

    if (link->next == null) {
        Button b = {};
        b.texture = plus;
        b.w = plus_w;
        b.h = plus_h;
        
        SDL_Color col = CustomColor(CustomOption::AccentColor);
        SDL_SetTextureColorMod(b.texture, col.r, col.g, col.b);
 
        if (TickButton(&b, end_x, y-plus_h/2+height/2).clicked) {
            MenuOperation operation = {};
            operation.op = OnLink;
            operation.hover.link = link;
            menu_insert_link_after(operation);
        }
    }

    if (link->selected) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderDrawRect(renderer, &link->link_visible_rect);
        SDL_RenderDrawRect(renderer, &link->desc_visible_rect);
    } else if (hover.link == link && link->highlighted) {
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
        if (a->collapsed) {
            cumh += 8;
            continue;
        }
        
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
    } else if (editing_field && s == State::CUSTOM_MENU) {
        buffer = editing_field->input;
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

static void DrawSelection(void) {
    //selection.box.y += (int)view_y;
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 120);
    SDL_RenderDrawRect(renderer, &selection.box);
    //selection.box.y -= (int)view_y;
}

static Link *FindHoveredLinkAt(Link *start, int x, int y) {
    if (!start) return null;

    for (Link *a = start;
         a;
         a = a->next)
    {
        if (PointIntersectsWithRect({x, y}, a->link_visible_rect)) {
            return a;
        }
        Link *result = FindHoveredLinkAt(a->child, x, y);
        if (result) return result;
    }

    return null;
}

#if 0
static Link *FindLinkBelowMouse(void) {
    Link *l = null;
    
    int x = mx, y = my+(int)view_y;
    
    int h;
    
    TTF_SizeText(font, "A", null, &h);
    
    h *= 2;
    
    while (l == null && abs(y - (my+(int)view_y)) < h) {
        l = FindHoveredLinkAt(start_link, x, y++);
    }
    
    return l;
}
#endif

// Find the hovered link, or the previous link at the mouse's position.
struct LinkResult {
    Link *hovered_link;
    Link *prev_link;
};
#if 0
static LinkResult FindHoveredLinkOrPreviousLink(void) {
    LinkResult result = {};
    
    result.hovered_link = FindHoveredLink(start_link).link;
    if (!result.hovered_link) {
        Link *l = FindLinkBelowMouse(); 
        if (l)
            result.prev_link = l->prev;
    }
    
    return result;
}
#endif

#if 0
static void AttemptDropSelection(void) {
    if (!selection.holding_link) return;
    
    LinkResult result = FindHoveredLinkOrPreviousLink();
    
    if (result.hovered_link) {
        // Add as a child link to result.hovered_link.
        Link *l = null;
        for (l = result.hovered_link->child;
             l && l->next;
             l = l->next);
        if (l) {
            l->next = selection.holding_link;
            selection.holding_link->prev = l;
        } else {
            result.hovered_link->child = selection.holding_link;
        }
        selection.holding_link->parent = result.hovered_link;
    } else {
        // Add in-between to the previous link
        if (result.prev_link) {
            //result.prev_link->next->prev = selection.holding_link;
            //result.prev_link->next = selection.holding_link;
            selection.holding_link->next = result.prev_link->next;
            selection.holding_link->prev = result.prev_link;
            selection.holding_link->parent = result.prev_link->parent; // Since they're at the same level.
            result.prev_link->next = selection.holding_link;
        }
    }
}
#endif

static void SaveToFile() {
    FILE *fp = fopen(filepath, "w");
    WriteLinksToFile(fp, start_link, 0);
    fclose(fp);
}

static void LoadFileRaw(FILE *fp) {
    while (!feof(fp)) {
        char link[MAX_STRING_SIZE] = {};
        fscanf(fp, "%[^\n]", link);
        
        if (feof(fp)) break;
        
        char c; // unsused, we just want to eat the \n
        fscanf(fp, "%c", &c);
        
        AddChildLink(null, link, "");
    }
}

static void LoadFile(const char *file) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        CommonFileErrorAndExit(file, config_path);
    }

    #define MAX_CHILD_LINKS 128
    Link *prevlinks[MAX_CHILD_LINKS] = {};

    unsigned long pos = ftell(fp);
    char c;
    fscanf(fp, "%c", &c);
    
    fseek(fp, pos, SEEK_SET);
    
    // Compatibility with raw files with links not in our format    
    
    if (c < '0' || c > '9') {
        LoadFileRaw(fp);
        fclose(fp);
        return;
    }
    
    while (!feof(fp)) {
        int layer;

        char link_and_desc[MAX_STRING_SIZE]={};

        fscanf(fp, "%d:%[^\n]\n", &layer, link_and_desc);

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

static void FreeEverything() {
    for (int i = 0; i < text_cache_count; i++) {
        if (text_cache[i].texture)
            SDL_DestroyTexture(text_cache[i].texture);
    }
    
    SDL_DestroyTexture(plus);
    SDL_DestroyTexture(arrow);

    FreeMenu(&global_menu);
    FreeLinks(start_link);

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void FirstTimeSetup() {
    NewFile((char*)filepath);
    
    SetupCustomMenu(true);
    
    int result = MessageBox(null, "Do you use chrome?\nOnly chrome and firefox are supported.", "Chrome?", MB_YESNO);
    if (result == IDYES) {
        CustomCheckbox(CustomOption::UseChrome) = 1;
    } else if (result == IDNO) {
        CustomCheckbox(CustomOption::UseChrome) = 0;
    } else {
        DeleteFileA(config_path);
        exit(1);
    }
    
    WriteConfig();
}

int RunGoodies() {
    win32_SetProcessDpiAware();
    
    char cwd[MAX_PATH]={};
    GetCurrentDirectory(MAX_PATH, cwd);

    SHGetFolderPathA(null, CSIDL_APPDATA, null, 0, config_path);
    
    sprintf(config_path, "%s\\goodies.cfg", config_path);
    
    bool exists = FileExists(config_path);
    
    if (exists) {
        SetupCustomMenu(false);
        LoadConfig();
    } else {
        FirstTimeSetup();
    }
    
    SetCurrentDirectory(cwd); // Windows is horrible and resets our CWD

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);

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
    
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    global_menu = MakeMenu();
    
    SDL_Surface *surf = IMG_Load("plus.png");
    plus = SDL_CreateTextureFromSurface(renderer, surf);
    plus_w = surf->w;
    plus_h = surf->h;
    SDL_FreeSurface(surf);
    
    surf = IMG_Load("arrow.png");
    arrow = SDL_CreateTextureFromSurface(renderer, surf);
    arrow_w = surf->w;
    arrow_h = surf->h;
    SDL_FreeSurface(surf);

    LoadFile(filepath);

    font = TTF_OpenFont(CustomField(CustomOption::Font).input, atoi(CustomField(CustomOption::FontSize).input));
    assert(font);

    title_font = TTF_OpenFont(CustomField(CustomOption::Font).input, atoi(CustomField(CustomOption::FontSize).input)+18);
    assert(title_font);

    bool running = true;

    SDL_Event event = {};
    bool has_event_queued = false;
    while (running) {
        mouse_clicked = false;
        typed_this_frame = false;
        global_should_update_text = false;
        
        if (has_event_queued)
            goto eventloop;
        
        while (SDL_PollEvent(&event)) {
            eventloop:
            switch (event.type) {
                case SDL_QUIT: { running = false; } break;
                case SDL_TEXTINPUT: {
                    typed_this_frame = true;
                    text_input_this_frame = *event.text.text;
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
                    } else if (state == State::CUSTOM_MENU && editing_field) {
                        strcat(editing_field->input, event.text.text);
                    }
                } break;
                case SDL_KEYDOWN: {
                    typed_this_frame = true;
                    switch (event.key.keysym.sym) {
                        case SDLK_PAGEDOWN: {
                            view_to_y += window_height;
                        } break;
                        case SDLK_PAGEUP: {
                            view_to_y -= window_height;
                        } break;
                        case SDLK_F8: {
                            custom_menu.active = !custom_menu.active;
                            if (custom_menu.active) {
                                state = State::CUSTOM_MENU;
                            } else {
                                state = State::NORMAL;
                                WriteConfig();
                            }
                        } break;
                        
                        case SDLK_UP: {
                            if (state == State::NORMAL)
                                view_to_y -= scroll_speed;
                        } break;
                        case SDLK_DOWN: {
                            if (state == State::NORMAL)
                                view_to_y += scroll_speed;
                        } break;
                        
                        case SDLK_ESCAPE: {
                            if (state == State::CUSTOM_MENU) {
                                custom_menu.active = false;
                                state = State::NORMAL;
                                WriteConfig();
                                break;
                            }
                            
                            ClearSelections();
                            
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
                            if (state == State::EDITING_NAME ||
                                state == State::EDITING_DESC)
                            {
                                state = State::NORMAL;
                                editing_link->editing = false;
                                editing_link = null;
                            } else if (editing_field && state == State::CUSTOM_MENU) {
                                UpdateTextField(editing_field);
                                editing_field->focus = false;
                                editing_field = null;
                            } else {
                                OpenSelectedLinksNoChildren(start_link);
                            }
                        } break;
                        
                        case SDLK_BACKSPACE: {
                            if (state != State::NORMAL) {
                                char *buffer = GetBufferFromState(state);
                                if (buffer && *buffer) {
                                    if (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]) {
                                        memset(buffer, 0, strlen(buffer));
                                    }
                                    buffer[strlen(buffer)-1]=0;
                                }
                            }
                        } break;
                        
                        case SDLK_c: {
                            if (selection.link_count) {
                                char *buffer = (char*)calloc(MAX_STRING_SIZE*selection.link_count, 1);
                                for (int i = 0; i < selection.link_count; i++){
                                    assert(selection.links[i]);
                                    strcat(buffer, selection.links[i]->link);
                                    strcat(buffer, "\n");
                                }
                                SDL_SetClipboardText(buffer);
                                free(buffer);
                            } else {
                                Hover h = FindHoveredLink(start_link);
                                if (h.link) {
                                    MenuOperation op = { OnLink, h };
                                    menu_copy_link(op);
                                }
                            }
                        } break;
                        
                        case SDLK_v: {
                            char *buffer = GetBufferFromState(state);
                            if (!buffer) {
                                Link *n = AddChildLink(null, "", "");
                                n->editing = true;
                                editing_link = n;
                                state = State::EDITING_NAME;
                                buffer = editing_link->link;
                            }
                            
                            if (buffer && (keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL]))
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
                case SDL_MOUSEMOTION: {
                    // NOTE: We no longer do dragging links
                    
                    
                    // This if statement occurs once upon initially
                    // moving the mouse when dragging a link.
#if 0
                    if (!selection.holding_link &&
                        selection.stored_x != -1 &&
                        (mx != selection.stored_x || my != selection.stored_y))
                    {
                        // Setup the link currently holding.
                        Link *l = selection.holding_link = FindHoveredLink(start_link).link;
                        
                        // Remove it's references to the link hierarchy.
                        if (l) {
                            if (l->prev) {
                                l->prev->next = l->next;
                            } else if (!l->parent) {
                                assert(l == start_link);
                                start_link = start_link->next;
                            }
                            
                            if (l->next) l->next->prev = l->prev;
                            if (l->parent) l->parent->child = l->next;
                            l->next = null;
                        }
                    }
#endif
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    ClearSelections();
                    selection.stored_x = mx;
                    selection.stored_y = my;
                } break;
                case SDL_MOUSEBUTTONUP: {
                    int button = event.button.button;
                    
                    if (selection.active) {
                        selection.box.x = selection.box.y = -1;
                        selection.box.w = selection.box.h = 0;
                        selection.active = false;
                        selection.stored_x = -1;
                        selection.stored_y = -1;
                        selection.holding_link = null;
                        break;
                    }
                    
                    selection.stored_x = -1;
                    selection.stored_y = -1;
                    selection.holding_link = null;
                    
                    if (button == SDL_BUTTON_LEFT)
                        mouse_clicked = true;
                    
                    if (state == State::NORMAL) {
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
        
        pmx = mx;
        pmy = my;
        mouse = SDL_GetMouseState(&mx, &my);
        keys = SDL_GetKeyboardState(0);
        
        view_y = lerp(view_y, view_to_y, scroll_t);
        
        if (mouse & SDL_BUTTON_LEFT &&
            selection.stored_x != mx &&
            selection.stored_y != my)
        {
            selection.active = true;
            
            selection.box.x = selection.stored_x;
            selection.box.y = selection.stored_y;
            selection.box.w = mx - selection.box.x;
            selection.box.h = my - selection.box.y;
            
            ClearSelections();
            SetSelectionLinks(start_link);
        }

        SDL_Color bgcol = CustomColor(CustomOption::BackgroundColor);
        SDL_SetRenderDrawColor(renderer, bgcol.r, bgcol.g, bgcol.b, 255);
        SDL_RenderClear(renderer);

        int pad = 0;
        
        TextDrawData data = {};
        if (*CustomField(CustomOption::TitleText).input) {
            strcpy(data.id, "title");
            strcpy(data.string, CustomField(CustomOption::TitleText).input);
            data.x = PAD;
            data.y = PAD - (int)view_y;
            data.font = title_font;
            data.force_redraw = false;
            data.wrap_width = window_width - PAD*2;
            data.color = CustomColor(CustomOption::AccentColor);
            
            TextDraw(renderer, &data);
            
            pad = 2*PAD;
        } else {
            pad = PAD;
        }
        
        DrawLinkAndChildLinks(start_link, PAD, pad+data.h);
        
        DrawSelection();

        SetAllLinksNotHighlighted(start_link);
        if (state == State::NORMAL) {
            hover = FindHoveredLink(start_link);
            if (hover.link) {
                hover.link->highlighted = true;
            }
        }
        
        if (selection.link_count) {
            TextDrawData text_data = {};
            strcpy(text_data.id, "ret");
            strcpy(text_data.string, "Press ENTER to Open Selection");
            text_data.font = font;
            
            int w, h;
            TTF_SizeText(text_data.font, text_data.string, &w, &h);
            
            text_data.x = window_width-w-PAD;
            text_data.y = window_height-h-PAD;
            
            double t = SDL_GetTicks()/100.0;
            
            double in = 0.5*(1+sin(t));
            in /= 4;
            in += 0.25;
            text_data.color = { (Uint8)(255*in), (Uint8)(255*in), 0, 255 };
            
            TextDraw(renderer, &text_data);
        } else {
            TextDrawData text_data = {};
            strcpy(text_data.id, "f8");
            strcpy(text_data.string, "Customize - F8");
            text_data.font = font;
            
            int w, h;
            TTF_SizeText(text_data.font, text_data.string, &w, &h);
            
            text_data.x = window_width-w-PAD;
            text_data.y = window_height-h-PAD;
            text_data.color = CustomColor(CustomOption::TextColor);
            text_data.color.r /= 2;
            text_data.color.g /= 2;
            text_data.color.b /= 2;
            
            TextDraw(renderer, &text_data);
        }
        
        DrawCustomMenu();

        DrawMenu(&global_menu);

        SDL_RenderPresent(renderer);

        Uint32 flag = SDL_GetWindowFlags(window);
        if (!(flag & SDL_WINDOW_INPUT_FOCUS)) {
            SDL_WaitEvent(&event);
            has_event_queued = true;
        }
    }

    SaveToFile();
    WriteConfig();

    return 0;
}

#ifdef RELEASE
int WINAPI WinMain(
        HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nShowCmd)
{
    (void)hInstance,hPrevInstance,lpCmdLine,nShowCmd;
    return RunGoodies();
}
#else
int main() {
    return RunGoodies();
}
#endif

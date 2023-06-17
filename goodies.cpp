// TODO:
//  - Click on link to open
//  - Click and drag to select multiple, then
//      - Hit enter to open multiple.
//  - Button to add new link(s)
//  - Auto-formats new links as separate items.
//  - Saves to file (file open menu)
//  - Customization options
//      - Description first, then link.
//      - Font & Size
//      - Colors
//      - Browser
//  - Right click item to open context menu.
//  - Click on description to edit it
//

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <assert.h>

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
    bool highlighted;
    SDL_Rect link_visible_rect,
             desc_visible_rect; // Gets set upon drawing.
    Link *next; // Next link at the same level
    Link *prev; // Previous link at the same level
    Link *child; // Next child link.
    Link *parent; // Parent link
};

static Link *start_link = NULL;
static Link *curr_link = NULL; // The top-level thing.
int link_count = 0;

static SDL_Window *window = null;
static SDL_Renderer *renderer = null;

static int window_width = 960, window_height = 960;

static TTF_Font *font = null;

Link *AddChildLink(Link *parent, const char *link, const char *description) {
    Link *new_link = null;
    
    if (!parent) {
        // Add to the end of the main list
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

int DrawLink(Link *link, int x, int y) {
    int link_w = 0;
    int height = 0;
    
    { // Link
        TextDrawData data = {};
        data.id = link->id*2;
        strcpy(data.string, link->link);
        data.x = x;
        data.y = y;
        data.font = font;
        data.force_redraw = false;
        data.wrap_width = window_width-PAD;
        data.color = SDL_Color{255, 255, 255, 255};
        
        TextDraw(renderer, &data);
        
        link->link_visible_rect = SDL_Rect{
            x, y,
            data.w, data.h
        };
        
        link_w += data.w + 16;
        height = data.h;
    }
    
    { // Description
        TextDrawData data = {};
        data.id = link->id*2+1;
        strcpy(data.string, link->description);
        data.x = x + link_w;
        data.y = y;
        data.font = font;
        data.force_redraw = false;
        data.wrap_width = window_width - PAD - link_w;
        data.color = SDL_Color{160, 60, 60, 255};
        
        TextDraw(renderer, &data);
        
        link->desc_visible_rect = SDL_Rect{
            x, y,
            data.w, data.h
        };
        
        if (data.h > height) height = data.h;
    }
    
    if (link->highlighted) {
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &link->link_visible_rect);
    }
    
    return height;
}

void SetAllLinksNotHighlighted(Link *start) {
    if (!start) return;
    
    for (Link *a = start;
         a;
         a = a->next)
    {
        a->highlighted = false;
        SetAllLinksNotHighlighted(a->child);
    }
}

// Find link that you're clicking and return it.
Link *FindClickedLink(Link *start, int mx, int my) {
    if (!start) return null;
    
    for (Link *a = start;
         a;
         a = a->next)
    {
        if (PointIntersectsWithRect({mx, my}, a->link_visible_rect)) {
            return a;
        }
        Link *result = FindClickedLink(a->child, mx, my);
        if (result) return result;
    }
    
    return null;
}

int DrawLinkAndChildLinks(Link *link, int x, int y) {
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

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance,hPrevInstance,lpCmdLine,nCmdShow;
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    
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
                                  0);
    assert(renderer);
    
    FILE *fp = fopen("d:/notes/test_goodies.txt", "r");
    while (!feof(fp)) {
        char buffer[MAX_STRING_SIZE] = {};
        fgets(buffer, MAX_STRING_SIZE, fp);
        
        size_t last = strlen(buffer)-1;
        if (buffer[last] == '\n')
            buffer[last] = 0;
        
        Link *new_link = AddChildLink(NULL, buffer, "Description");
        AddChildLink(new_link, "Sublink A", "Description");
        AddChildLink(new_link, "Sublink B", "Description 1");
        AddChildLink(new_link, "Sublink C", "Description 2");
    }
    fclose(fp);
    
    font = TTF_OpenFont("C:/Windows/Fonts/bookosi.ttf", 22);
    assert(font);
    
    TTF_Font *title_font = TTF_OpenFont("C:/Windows/Fonts/bookosi.ttf", 40);
    assert(title_font);
    
    bool running = true;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: { running = false; } break;
                case SDL_KEYDOWN: {
                    switch (event.key.keysym.sym) {
                        case SDLK_ESCAPE: { running = false; } break;
                    }
                } break;
                case SDL_WINDOWEVENT: {
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        window_width = event.window.data1;
                        window_height = event.window.data2;
                    }
                } break;
                case SDL_MOUSEBUTTONDOWN: {
                    if (event.button.button == SDL_BUTTON_LEFT) {
                    }
                } break;
            }
        }
        
        int mx, my;
        Uint32 mouse = SDL_GetMouseState(&mx, &my);
        (void)mouse;
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        TextDrawData data = {};
        data.id = link_count;
        strcpy(data.string, "Goodies");
        data.x = PAD;
        data.y = PAD;
        data.font = title_font;
        data.force_redraw = false;
        data.wrap_width = window_width - PAD*2;
        data.color = SDL_Color{200, 40, 20, 255};
        
        TextDraw(renderer, &data);
        
        DrawLinkAndChildLinks(start_link, PAD, 2*PAD+data.h);
        
        SetAllLinksNotHighlighted(start_link);
        Link *link = FindClickedLink(start_link, mx, my);
        if (link) {
            link->highlighted = true;
        }
        
        SDL_RenderPresent(renderer);
    }
    
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    
    SDL_Quit();
    TTF_Quit();
    
    return 0;
}
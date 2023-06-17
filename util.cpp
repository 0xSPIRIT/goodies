#define MAX_STRING_SIZE 1024
#define MAX_TEXT_COUNT 2048

struct TextDrawData {
    size_t id;
    char string[MAX_STRING_SIZE];
    int x, y;
    TTF_Font *font;
    bool force_redraw;
    Uint32 wrap_width;
    
    SDL_Color color;
    
    SDL_Texture *texture; // Out
    int w, h; // Out
};

static TextDrawData text_cache[MAX_TEXT_COUNT] = {};
static int text_cache_count = 0;

static TextDrawData *FindTextDataInCache(size_t id) {
    for (int i = 0; i < text_cache_count; i++) {
        if (text_cache[i].id == id) return &text_cache[i];
    }
    return null;
}

static void AddTextDataToCache(TextDrawData *a) {
    text_cache[text_cache_count++] = *a;
}

static bool HasTextDataChanged(TextDrawData *a, TextDrawData *b) {
    if (a->wrap_width != b->wrap_width) return true;
    if (strcmp(a->string, b->string) != 0) return true;
    if (memcmp(&a->color, &b->color, sizeof(SDL_Color)) != 0) return true;
    
    return false;
}

static void TextDraw(SDL_Renderer *renderer, TextDrawData *data) {
    bool should_redraw = data->force_redraw;
    TextDrawData *cache_object = FindTextDataInCache(data->id);
    
    if (!cache_object) {
        should_redraw = true;
    } else if (!should_redraw) {
        bool has_changed = HasTextDataChanged(data, cache_object);
        if (has_changed)
            should_redraw = true;
    }
    
    if (!should_redraw) {
        data->w = cache_object->w;
        data->h = cache_object->h;
        data->texture = cache_object->texture;
        const SDL_Rect dst = {
            data->x, data->y,
            data->w, data->h
        };
        SDL_RenderCopy(renderer, data->texture, NULL, &dst);
        return;
    }
    
    SDL_Surface *surface = TTF_RenderText_Blended_Wrapped(data->font,
                                                          data->string,
                                                          data->color,
                                                          data->wrap_width);
    if (!surface) {
        // Place dummy values as the width and height,
        // so it'll appear as an empty line, and not
        // draw nothing.
        TTF_SizeText(data->font, "A", &data->w, &data->h);
        data->texture = null;
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    
    data->texture = texture;
    data->w = surface->w;
    data->h = surface->h;
    
    if (cache_object) {
        *cache_object = *data;
    } else {
        AddTextDataToCache(data);
    }
    
    const SDL_Rect dst = {
        data->x, data->y,
        data->w, data->h
    };
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    
    SDL_FreeSurface(surface);
}

//~ Other stuff

inline static bool PointIntersectsWithRect(SDL_Point a, SDL_Rect r) {
    if (a.x >= r.x && a.y >= r.y &&
        a.x <= r.x+r.w && a.y <= r.y+r.h)
        return true;
    return false;
}
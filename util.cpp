#define MAX_STRING_SIZE 1024
#define MAX_TEXT_COUNT 2048

struct TextDrawData {
    char id[32];
    char string[MAX_STRING_SIZE];
    int x, y;
    TTF_Font *font;
    bool force_redraw;
    bool wrapped;
    Uint32 wrap_width;

    SDL_Color color;

    SDL_Texture *texture; // Out
    int w, h; // Out
};

TextDrawData text_cache[MAX_TEXT_COUNT] = {};
int text_cache_count = 0;
bool global_should_update_text = false;

TextDrawData *FindTextDataInCache(char id[32]) {
    for (int i = 0; i < text_cache_count; i++) {
        if (strcmp(text_cache[i].id, id) == 0) return &text_cache[i];
    }
    return null;
}

void AddTextDataToCache(TextDrawData *a) {
    text_cache[text_cache_count++] = *a;
}

bool HasTextDataChanged(TextDrawData *a, TextDrawData *b) {
    if (a->font != b->font) return true;
    if (a->wrapped != b->wrapped) return true;
    if (a->wrap_width != b->wrap_width) return true;
    if (strcmp(a->string, b->string) != 0) return true;
    if (memcmp(&a->color, &b->color, sizeof(SDL_Color)) != 0) return true;

    return false;
}

void TextDraw(SDL_Renderer *renderer, TextDrawData *data) {
    bool should_redraw = data->force_redraw || global_should_update_text;
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

    if (cache_object && cache_object->texture) {
        SDL_DestroyTexture(cache_object->texture);
    }

    SDL_Surface *surface = null;

    if (data->wrapped) {
        surface = TTF_RenderText_Blended_Wrapped(data->font,
                                                 data->string,
                                                 data->color,
                                                 data->wrap_width);
    } else {
        surface = TTF_RenderText_Blended(data->font,
                                         data->string,
                                         data->color);
    }

    if (!surface) {
        // Place dummy values as the width and height,
        // so it'll appear as an empty line, and not
        // draw nothing.

        TTF_SizeText(data->font, "|", &data->w, &data->h);
        data->texture = null;
        if (cache_object)
            *cache_object = *data;
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

inline bool PointIntersectsWithRect(SDL_Point a, SDL_Rect r) {
    if (a.x >= r.x && a.y >= r.y &&
        a.x <= r.x+r.w && a.y <= r.y+r.h)
        return true;
    return false;
}

inline bool RectIntersectsWithRect(SDL_Rect r1, SDL_Rect r2) {
    bool noOverlap = r1.x >= r2.x+r2.w ||
        r2.x >= r1.x+r1.w ||
        r1.y >= r2.y+r2.h ||
        r2.y >= r1.y+r1.h;
    return !noOverlap;
}

inline float lerp(float a, float b, float t) {
    return a+(b-a)*t;
}

inline void NewFile(char *out) {
    OPENFILENAME ofn = {};
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = null;
    ofn.lpstrTitle = "Create or open a file to store links";
    ofn.lpstrFilter = "*.txt";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER;

    if (GetOpenFileName(&ofn))
        strcpy(out, fileName);
    else
        exit(0);
}

BOOL FileExists(LPCTSTR szPath) {
    DWORD dwAttrib = GetFileAttributes(szPath);
    
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && 
            !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void CommonFileErrorAndExit(const char *file, const char *config_path) {
    char message[MAX_STRING_SIZE] = {};
    sprintf(message, "Couldn't load file \"%s\"\nWe'll delete your config file, then exit.\n\nReopen goodies to re-choose your links file.", file);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error!", message, 0);
    DeleteFile(config_path);
    exit(1);
}
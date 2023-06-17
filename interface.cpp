struct Button {
    SDL_Texture *texture;
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
    
#ifndef GAME_SPRITES_H
#include <SDL2/SDL.h>

struct Game_Sprite{
    double worldX = 0;
    double worldY = 0;
    double width = 0;
    double height = 0; //position and size in world
    int texID = 0; //index of sprite texture
    int frame = 0; //animation frame number
    int totalFrames = 0; //number of animation frames
    SDL_Rect image{0, 0, 0, 0}; //information about its texture image
    bool visible = false; //can it be seen by player
    bool solid = false; //can player walk through it
    bool pickup = false; //should it destroy on collision with player    
};
#endif
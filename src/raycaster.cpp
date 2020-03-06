#include <SDL2/SDL.h> //SDL main library functions
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#include <stdio.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include "blocktypes.h" //world map blocks
#include "game_sprites.h" //objects

//some constants for handling files on different operating systems
#ifdef _WIN32
    const char PATH_SYM = '\\';
#else
    const char PATH_SYM = '/';
#endif

//some const values for our screen resolution
bool debugColors = false; //toggle rendering textures or just draw world geometry with plain colors
bool ceilingOn = false; //toggle drawing ceiling tiles or skybox when textures are turned on
bool enableInput = false; //can temporarily turn off player input (does not affect most debug hotkeys)

int mapWidth = 24; //setup some defualt map dimensions. will be overridden when loading the level
int mapHeight = 24;

//really shitty way to set internal rendering dimensions.
//TODO: stop hard coding screen dimensions. load from config file instead
const int gscreenWidth =  960;//1920;//1280; //640; //720; //800; //960;
const int gscreenHeight = 540;//1080;//720; //360; //405; //450; //540;

bool vertSyncOn = true;
bool letterboxOn = true;

double worldFog = 0.0; // 0 to 1. 0 is thickest, 1 is non-existent. World-wide fog minimum density.
double playerFog = 1.0; // 0 - 1. 0 is thickest, 1 is non-existent. Fog density starting at the player.
double fogMultiplier = 1; // Multiplied by distance from player. Adjusts the distance at which the fog transitions from player to global levels
SDL_Color fogColor = {0,0,0,0}; // RGBA values for fog. Alpha is ignored and determined by the above values
bool fogOn = false; //toggled fog effect on/off for performance
double brightSin[gscreenWidth]; //fog distance is in relation to viewing plane
                                //this lookup table is used to curve it and give better effect

//some color values for convenience
const SDL_Color cRed = {255,0,0,255};
const SDL_Color cGreen = {0,255,0,255};
const SDL_Color cBlue = {0,0,255,255};
const SDL_Color cYellow = {255,255,0,255};
const SDL_Color cCyan = {0,255,255,255};
const SDL_Color cMagenta = {255,0,255,255};
const SDL_Color cWhite = {255,255,255,255};
const SDL_Color cBlack = {0,0,0,255};

SDL_Rect miniMapRect = {gscreenWidth - mapWidth - gscreenWidth/16,  //draw the background of the minimap
                        gscreenHeight / 16,
                        mapWidth,
                        mapHeight};
SDL_Rect miniMapDot = {miniMapRect.x, miniMapRect.y, 2, 2}; //for drawing dots on the minimap
bool mapOn = false; //toggle mini map on/off

double floorDist[gscreenHeight];
//double ceilDist[gscreenHeight];

const double radToDeg = 180 / M_PI; //convenience numbers
const double degToRad = M_PI / 180;

double hFOV = 90.0;  //horizontal field of view in degrees
//double vFOV = (hFOV/gscreenWidth * gscreenHeight) / (std::atan((double)gscreenHeight/(double)gscreenWidth)*radToDeg*2.0);
double vFOV = 1.0; //not really used right now, but could be used to alter wall height
double vertLook = 0;  // number of pixels to look up/down. in screen coordinates
double vertHeight = 0; //center of the screen (offset in screen coordinates)
double vertSpeed = 0.1; //multiplier to control player's change in vertHeight (framerate independent)

bool sprinting = false;



//player position and screen plane coordinates:
//
//posX, posY are player position in world coordinates. each "1" is a full world block
//dirX, dirY are relative to player position and indicate the direction the player is facing
//planeX, planeY relative to dirX, dirY and used for player perspective projection (screen) plane
//
//relationship of dir vector length to plane vector length will change fov
//
//
//         this length controlled by FOV
//             |
//             v
//         *===========>* dirX, dirY
//     posX,posY        |
//                      | <-this length static value 1
//                      |
//                      * planeX, planeY
//
//
double planeX = 0, planeY = 1;
double posX = 2, posY = 2;         //x and y start position, overridden during map load
double dirX = std::tan((hFOV * degToRad)/2), dirY = 0;         //initial direction vector is east (0 degrees)

double moveSpeed = 0; //multiplier for frame rate independent movement speed

double viewTrip = 0.0; //fun effect to stretch and curve floor/ceiling. totally useless. fun side effect of trying to get floor casting math right

double blockAheadDist = 500; //distance to the nearest block straight ahead of player. reset durign each raycast calculation loop
int blockAheadX = 0, blockAheadY = 0; //world coordinates of the nearest block that's straight ahead
int blockLeftX = 0, blockLeftY = 0; //same but this is the block furthest to the player's left that's on screen. used for minimap only
int blockRightX = 0, blockRightY = 0; //block furthest right that's on screen. used for minimap only

double mouseSense = 0.25; //horizontal mouse sensitivity multiplier
double mouseVertSense = 0.75; //vertical mouse sensitivity multiplier

//some numbers for frame time calculation. used for frame rate independence and performance calculations
Uint64 oldtime = 0;
Uint64 gtime = 0;
Uint64 gDeltaTimer = 0;
unsigned int framecounter = 0;

//game window
SDL_Window *gwindow = NULL;

//The window renderer. hardware accelerated backend
SDL_Renderer *gRenderer = NULL;
SDL_Texture *gcurrTex = NULL; //the texture we're actually currently copying from during render
SDL_Texture **gwallTex = NULL;//wall textures
SDL_Texture *gskyTex = NULL; //skybox texture
SDL_Texture *gfloorTex = NULL; //floor texture
SDL_Texture *gceilTex = NULL; //ceiling texture
SDL_Texture *gfloorBuffer = NULL; //buffer texture. calculated perspective mapping of the floor and ceiling will be plotted onto this buffer
SDL_Texture *gfogTex = NULL; //buffer texture. calculated fog will be plotted onto this texture
SDL_Texture *weaponTex = NULL; //current player weapon (from first person perspective)
SDL_Texture **pickupTex = NULL;
SDL_Texture **maskTex = NULL;

const int totalWallTextures = 3; //number of unique wall textures. needs to be read from a config or dynamically calculated
const int totalPickupTextures = 4;

SDL_Rect gskyDestRect; //used for skybox. where (on screen) to draw the skybox
SDL_Rect gskySrcRect; //used for skybox. where (on skybox texture) to grab current skybox from
SDL_Rect gfloorRect; //only used for debug colors. determines where to draw floor color on screen
SDL_Rect weaponTexRect = {0,0,0,0}; //source rectangle. where (on weapons texture) to get current weapon image
SDL_Rect weaponDestRect; //where (on screen) to draw weapon


std::stringstream ssFPS; //string for window title. currently used for debug info (FPS, FOV, etc)

std::vector<std::vector<Map_Block>> leveldata; //current map information as a 2d dynamic size array

std::vector<Game_Sprite> allSprites;
std::vector<double> spriteDistances;
std::vector<int> spriteOrder;

bool init(); //basic start-SDL stuff
bool initWindow(); //get window and hardware accelerated (if possible) renderer
bool initTextures(); //load in assets and make textures from them all
void initAllSprites();
void newlevel(bool warpView); //reset some basic settings and load another level
void loadLevel(std::string path); //read in map data and populate leveldata array with it
bool update(); //update world 1 tick
void calcDeltaTime();
void updateWindowTitle();
bool handleInput(); //react to player input.
void updateScreen(); //draw stuff
void updateBlockTimers(int x, int y, int radius, double percent);
void calcRaycast(); //calculate all raytracing. calls draw world when it's done
void calcFloorDist();
void drawWorldGeoFlat(double* wallDist, int* side, int* mapX, int* mapY); //draw world with debug colors
void drawWorldGeoTex(double* wallDist, int* side, int* mapX, int* mapY); //draw world with textures
void drawFloor(double* wallDist, int* drawStart, int* drawEnd, int* side, int* mapX, int* mapY); //calculate and draw perspective floor and ceiling
void drawFloor();
void drawMiniMap(); //draw little debug color minimap
void drawSkyBox(); //paste a skybox
void drawSprites(double* wallDist);
void close(); //prepare to quit game
SDL_Texture *loadTexture(const std::string &file, SDL_Renderer *ren); // loads a BMP image into a texture on the rendering device
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, SDL_Rect dst, SDL_Rect *clip); // draw an SDL_texture to an SDL_renderer at position x,y
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y, SDL_Rect *clip);
std::string getProjectPath(const std::string &subDir);//get working directory, account for different folder symbol in windows paths
SDL_Texture *loadImage(std::string path);//load BMP, return texture
SDL_Texture *loadImageColorKey(std::string path, SDL_Color transparent);//load BMP with color key transparency, return texture
void generatefogMask(SDL_Texture* tex, int* drawStart, int* drawEnd, double* wallDist); //calculate fog using current settings and populate texture with results
void drawHud(); //just calls the various HUD related draw commands
void drawWeap(); //paste current player weapon on screen
void changeFOV(bool rel, double newFOV); //alters player camera FOV by changing length of direction vector
void resizeWindow(bool letterbox);

int main(int argc, char **argv)
{

    //init SDL
    if (init())
    {
        newlevel(false);   
        //Main loop flag
        bool quit = false;
        while (!quit)
        {
            quit = update();
        }

        close();
    }
    else
    {
        close();
        printf("Game initialization failed. Something crucial broke. Rage quitting.\n");
    }
    return 0;
}

bool init()
{
    bool success = true;

    

    if (SDL_Init(SDL_INIT_VIDEO) < 0) //init video, if fail, print error
    {
        success = false;
        printf("SDL failed to initialize. SDL_Error: %s\n", SDL_GetError());
    }
    else //successful SDL init
    {
        if (!initWindow())
        {
            printf("Window failed to initialize. SDL Error: %s\n", SDL_GetError());
            success = false;
        }
    }
    if (!initTextures())
    {
        success = false;
    }
    initBlockTypes();

    return success;
}

bool initWindow()
{
    bool success = true;
    //create window
    gwindow = SDL_CreateWindow("Raycast Test. FPS: ", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gscreenWidth, gscreenHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (gwindow == NULL) //window failed to create?
    {
        success = false;
        printf("SDL window creation unsuccessful. SDL_Error: %s\n", SDL_GetError());
    }
    else //successful window creation
    {
        //Create renderer for window
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl"); //hopefully use opengl
        SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1"); //turn on SDL 2.0.10+ built-in draw batching for slightly faster and more consistent draw speeds
                                                    //if, for some reason, SDL version is <2.0.10 it'll crash here. that's fine.
        gRenderer = SDL_CreateRenderer(gwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
        if (gRenderer == NULL)
        {
            printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
            #ifdef _WIN32
            //this nonsense is because of a bug with windows 10
            //without it, the mouse won't actually be captured
            //the relative input will work, but until the user minimizes and restores the window, it isn't captured
            //idk why and there's a good chance it'll be magicaly resolved one day without me knowing
            SDL_MinimizeWindow(gwindow);
            SDL_RaiseWindow(gwindow);
            SDL_RestoreWindow(gwindow);
            #endif
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0"); //explicitly request nearest neighbor scaling
            gfloorRect.x = 0;
            gfloorRect.y = gscreenHeight / 2;
            gfloorRect.w = gscreenWidth;
            gfloorRect.h = gscreenHeight / 2;
            gskyDestRect.x = 0;
            gskyDestRect.y = 0;
            gskyDestRect.w = gscreenWidth;
            gskyDestRect.h = gscreenHeight;//  /2;
            SDL_SetRelativeMouseMode(SDL_bool(true)); // lock the cursor to the window, now that we have focus

            SDL_RendererInfo rendererInfo;
            SDL_GetRendererInfo(gRenderer, &rendererInfo);
            printf("Render Driver: %s %s",rendererInfo.name,"\n");
        }
    }
    return success;
}

bool initTextures()
{
    bool success = true;

    // loading in the textures
    std::stringstream texFileName;
    texFileName << "resources" << PATH_SYM << "textures" << PATH_SYM << "sky.bmp"; 
    gskyTex = loadImage(texFileName.str());
    texFileName.str(std::string());
    if (gskyTex == NULL)
    {
        success = false;
    }
    else /* TODO:  fix gskySrcRect magic numbers */
    {
        int skyw, skyh;
        Uint32 skyf;
        SDL_QueryTexture(gskyTex, &skyf, NULL, &skyw, &skyh);
        gskySrcRect.w = skyw/4;// one fifth of sky box bitmap width, therefore 90 degrees
        gskySrcRect.h = skyh/2; //tried to calculate out half the view plane height in degrees, and applied to the height of the sky
                        //with the entire sky bmp height representing 90 degrees, then converted that to pixels
        gskySrcRect.y = skyh/4;  // adjust down 220 sky bmp height - the 137 pixels for viewing. bottom of sky bmp is horizon, top is out of view

    }
    
    
    texFileName << "resources" << PATH_SYM << "textures" << PATH_SYM << "floor0.bmp"; 
    gfloorTex = loadImage(texFileName.str());
    texFileName.str(std::string());
    if (gfloorTex == NULL)
    {
        success = false;
    }
    texFileName << "resources" << PATH_SYM << "textures" << PATH_SYM << "ceil0.bmp";
    gceilTex = loadImage(texFileName.str());
    texFileName.str(std::string());
    if (gceilTex == NULL)
    {
        success = false;
    }
    gwallTex = new SDL_Texture *[totalWallTextures];
    for(int i = 0; i < totalWallTextures; i++)
    {
        texFileName.str(std::string());
        texFileName << "resources" << PATH_SYM << "textures" << PATH_SYM << "wall" << i << ".bmp";
        gwallTex[i] = loadImage(texFileName.str());
        if (gwallTex[i] == NULL)
        {
            success = false;
        }
    }

    pickupTex = new SDL_Texture *[totalPickupTextures];
    for(int i = 0; i < totalPickupTextures; i++)
    {
        texFileName.str(std::string());
        texFileName << "resources" << PATH_SYM << "sprites" << PATH_SYM << "pickup" << i << ".bmp";
        pickupTex[i] = loadImageColorKey(texFileName.str(), cMagenta);
        if (pickupTex[i] == NULL)
        {
            success = false;
        }
    }

    maskTex = new SDL_Texture *[totalPickupTextures];
    for(int i = 0; i < totalPickupTextures; i++)
    {
        texFileName.str(std::string());
        texFileName << "resources" << PATH_SYM << "sprites" << PATH_SYM << "mask" << i << ".bmp";
        maskTex[i] = loadImageColorKey(texFileName.str(), cMagenta);
        if (maskTex[i] == NULL)
        {
            success = false;
        }
    }

    texFileName.str(std::string());
    texFileName << "resources" << PATH_SYM << "sprites" << PATH_SYM << "shotgun1.bmp";
    weaponTex = loadImageColorKey(texFileName.str(), cMagenta);
    if (weaponTex == NULL)
    {
        success = false;
    }
    else
    {
        int tw, th;
        double scale;
        SDL_QueryTexture(weaponTex, NULL, NULL, &tw, &th);
        weaponTexRect={0,0,tw,th};
        scale = (gscreenWidth/2.0)/tw; //set gun to approx the size of the lower right quadrant of the screen
        tw *= scale;
        th *= scale;
        weaponDestRect = {std::max(gscreenWidth-tw,0),std::max(gscreenHeight-th,0),tw,th};
    }
    
    SDL_RendererInfo info;
    SDL_GetRendererInfo(gRenderer,&info);
    gfloorBuffer = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, gscreenWidth, gscreenHeight);
    if (gfloorBuffer == NULL)
    {
        success = false;
    }
    gfogTex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, gscreenWidth, gscreenHeight);
    if(gfogTex == NULL)
    {
        success = false;
    }
    else
    {
        calcFloorDist();
        for(int x = 0; x < gscreenWidth; x++) //setup a sin lookup table for the fog mask for later
        {
            brightSin[x] = 1/sin((M_PI / 2.0)-(hFOV / 2.0 * degToRad)+(((double)x/(double)gscreenWidth)*(hFOV * degToRad)));
            //brightSin[x] *= brightSin[x]; // squared to get stronger curve effect
        }
    }
    return success;
}

void initAllSprites() //just a shitty test function to spawn 20 of the same object. this is just to varify we CAN draw sprites
{
    int totalSprites = 20;
    int eachType = 5;
    allSprites.clear();
    for(int n = 0; n < totalPickupTextures; n++)
    {
        for(int i = n*eachType; i < std::min((n+1)*eachType, totalSprites); i++)
        {
            allSprites.push_back(Game_Sprite());
            allSprites[i].texID = n;
            allSprites[i].worldX = posX - 2.5 + 5.0 * ((double) rand() / (RAND_MAX));
            allSprites[i].worldY = posY - 2.5 + 5.0 * ((double) rand() / (RAND_MAX));
            SDL_QueryTexture(pickupTex[allSprites[i].texID], NULL, NULL, & allSprites[i].image.w, & allSprites[i].image.h);
            allSprites[i].width = 0.1*(n+1);
            allSprites[i].height = std::min(1.0, allSprites[i].width * (allSprites[i].image.h / allSprites[i].image.w));
            allSprites[i].image.x = 0;
            allSprites[i].image.y = 0;
            
        }
    }


    spriteDistances.resize(totalSprites);
    spriteOrder.resize(totalSprites);    
}

bool update()
{
    updateScreen();
    calcDeltaTime();
    bool quit = handleInput();
    updateBlockTimers(int(posX), int(posY), 2, -2.0 * (gDeltaTimer / (double)SDL_GetPerformanceFrequency())); //tell nearby doors to open
    updateWindowTitle();
    return quit;
}

void calcDeltaTime()
{
    gtime = SDL_GetPerformanceCounter();
    gDeltaTimer = (gtime - oldtime);
    oldtime = gtime;
}

void updateWindowTitle()
{
    if(++framecounter >= SDL_GetPerformanceFrequency() / gDeltaTimer / 5)
    {
        framecounter = 0;
        ssFPS << "Raycast Test. FPS: " << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << (double)SDL_GetPerformanceFrequency() / gDeltaTimer
              << " | Vsync: "          << vertSyncOn
              << " | Timer: "          << leveldata[blockAheadX][blockAheadY].timer
              << " | hFOV: "           << std::fixed << std::setprecision(1) << std::setfill('0') << std::setw(3) << hFOV
              << " | Height: "         << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(4) << vertHeight;
        SDL_SetWindowTitle(gwindow, ssFPS.str().c_str());
        ssFPS.str(std::string()); //blank the stream
    }
}

void updateScreen()
{
    //clear screen with a dark grey
    SDL_SetRenderDrawColor(gRenderer, 0x20, 0x20, 0x20, 0xff);
    SDL_RenderClear(gRenderer);
    calcRaycast(); //calculates and draws all raycast related screen updates
    drawHud();
    //Update screen
    SDL_RenderFlush(gRenderer); //draw all batched commands
    SDL_RenderPresent(gRenderer); //blit back-buffer to screen
}

void updateBlockTimers(int inX, int inY, int radius, double percent)
{
    for(int y = std::max(inY - radius, 0); y < std::min(mapHeight, inY + radius + 1); ++y)
    {
        for(int x = std::max(inX - radius, 0); x < std::min(mapWidth, inX + radius + 1); ++x)
        {
            if(leveldata[x][y].timerOn)
            {
                leveldata[x][y].timer += percent;
                if(leveldata[x][y].timer < 0.0)
                {
                    leveldata[x][y].timer = 0.0;
                    leveldata[x][y].timerOn = false;
                    leveldata[x][y].visible = false;
                    leveldata[x][y].solid = false;
                }
                else if(leveldata[x][y].timer > 1.0)
                {
                    leveldata[x][y].timer = 1.0;
                    leveldata[x][y].timerOn = false;
                }
            }
        }
    }
}

void calcRaycast()
{
    //buffers to hold some results from raycasting for each x across screen
    double wallDist[gscreenWidth]; //dist to nearest wall
    int side[gscreenWidth];    //was a NS or a EW wall hit?
    int mapX[gscreenWidth];    //the x value on map of wall hit
    int mapY[gscreenWidth];    //the y value on map of wall hit

    double cameraX, rayDirX, rayDirY, sideDistX, sideDistY, deltaDistX = 0, deltaDistY = 0, perpWallDist;
    int stepX, stepY, hit;    
    //ACTUAL RAYCAST LOGIC
    for (int x = 0; x < gscreenWidth; x++)
    {

        //calculate ray position and direction
        cameraX = 2 * x / double(gscreenWidth) - 1; //x-coordinate in camera space, or along the x of the camera plane itself
                                                    //cameraX ranges from -1 to 1, with 0 being center of camera screen
        
        rayDirX = dirX + planeX * cameraX; //the XY coord where the vector of this ray crosses the camera plane
        rayDirY = dirY + planeY * cameraX; //
        //which box of the map we're in
        mapX[x] = int(posX);
        mapY[x] = int(posY);


        //length of ray from one x or y-side to next x or y-side
        if(rayDirX != 0)
        {
            deltaDistX = std::abs(1 / rayDirX); //the x component of a vector in the player's viewing direction that spans exactly across 1 map block side to side
        }
        
        
        if(rayDirY != 0)
        {
            deltaDistY = std::abs(1 / rayDirY); //y component of that vector
        }
        

        //calculate step and initial sideDist
        if (rayDirX < 0)
        {
            stepX = -1; //facing left, step left (decrement) through map matrix x
            sideDistX = (posX - mapX[x]) * deltaDistX; // x component of viewing distance vector to nearest wall
                                                    //calculated by getting our fractional perpendicular offset from the closest wall, times the x component 
                                                    //of the length of the vector in our viewing direction that spans exactly 1 whole map block side-to-side
        }
        else
        {
            stepX = 1; //step right (increment) through map matrix x
            sideDistX = (mapX[x] + 1.0 - posX) * deltaDistX; // x component of distance vector to nearest wall
        }
        if (rayDirY < 0)
        {
            stepY = -1; //facing up, step up (decrement) through map matrix y
            sideDistY = (posY - mapY[x]) * deltaDistY; //y component of distance vector to nearest wall
        }
        else
        {
            stepY = 1; //facing down, step down (increment) through map matrix y
            sideDistY = (mapY[x] + 1.0 - posY) * deltaDistY; //y component of distance to nearest wall
        }

        hit = 0; //was there a wall hit?

        //perform DDA
        while (hit == 0)
        {
            //jump to next map square in x-direction, OR in y-direction
            if (sideDistX < sideDistY)
            {
                sideDistX += deltaDistX;
                mapX[x] += stepX;
                side[x] = 0; 
            }
            else
            {
                sideDistY += deltaDistY;
                mapY[x] += stepY;
                side[x] = 1;
            }
            //Check if ray has hit a wall
            if (leveldata[mapX[x]][mapY[x]].visible)
            {
                if(leveldata[mapX[x]][mapY[x]].isDoor) //sliding door, so check if the door is blocking or not
                {
                    double wallX, checkDist;

                    if (side[x] == 0) //NS wall
                    {
                        perpWallDist = (mapX[x] + ((double)stepX * 0.5) - posX + (1 - stepX) / 2) / rayDirX;

                        checkDist = (mapY[x] + stepY - posY + (1 - stepY) / 2) / rayDirY;

                        wallX = posY + perpWallDist * rayDirY;
                    }
                    else  //EW wall
                    {
                        perpWallDist = (mapY[x] + ((double)stepY * 0.5) - posY + (1 - stepY) / 2) / rayDirY;

                        checkDist = (mapX[x] + stepX - posX + (1 - stepX) / 2) / rayDirX;

                        wallX = posX + perpWallDist * rayDirX;
                    }
                    wallX -= floor((wallX)); //we've determined the where (0 to 1) across the wall that we hit
                                            //compare that to this wall's timer to see if we hit or keep going
                    
                    if(checkDist > perpWallDist)
                    if(wallX <= leveldata[mapX[x]][mapY[x]].timer)
                    {
                        hit = 1;
                        wallDist[x] = perpWallDist;
                    }                
                }
                else //not a sliding door, definitely a hit
                {
                    hit = 1;

                            //Calculate distance to wall projected on camera direction (Euclidean distance will give fisheye effect!)
                    if (side[x] == 0)
                    {
                        perpWallDist = (mapX[x] - posX + (1 - stepX) / 2);
                        perpWallDist = perpWallDist / rayDirX;
                    }
                    else
                    {
                        perpWallDist = (mapY[x] - posY + (1 - stepY) / 2);
                        perpWallDist = perpWallDist / rayDirY;
                    }

                    wallDist[x] = perpWallDist; //fill wall distance buffer
                }          
            }
        }
        //store location and distance of wall straight ahead of player
        if(x == gscreenWidth / 2)
        {
            if (side[x] == 0)
            {
                blockAheadDist = std::abs(perpWallDist * rayDirX);
            }
            else
            {
                blockAheadDist = std::abs(perpWallDist * rayDirY);
            }            
            blockAheadX = mapX[x];
            blockAheadY = mapY[x];
        }
        else if(x == 0) //store location of block that's in our leftmost periphery
        {        
            blockLeftX = mapX[x];
            blockLeftY = mapY[x];
        }
        else if(x == gscreenWidth -1) //store location of block that's in our rightmost periphery
        {        
            blockRightX = mapX[x];
            blockRightY = mapY[x];
        }

    }
    if(debugColors)
        drawWorldGeoFlat(wallDist, side, mapX, mapY);
    else
        drawWorldGeoTex(wallDist, side, mapX, mapY);
    drawSprites(wallDist);
}

void calcFloorDist()
{

    for(int y = 0; y < ((gscreenHeight / 2) + vertLook); y++)
    {
        // Current y position compared to the center of the screen (the horizon)
        int p = y - ((gscreenHeight / 2) + vertLook);

        // Vertical position of the camera.
        double posZ = (gscreenHeight / 2.0) - (vertHeight * gscreenHeight);

        // Horizontal distance from the camera to the floor for the current row.
        // 0.5 is the z position exactly in the middle between floor and ceiling.
        double rowDistance = -posZ / p;

        floorDist[y] = rowDistance;
    }
    for(int y = ((gscreenHeight / 2) + vertLook); y < gscreenHeight; y++)
    {
        // Current y position compared to the center of the screen (the horizon)
        int p = y - ((gscreenHeight / 2) + vertLook);
        // Vertical position of the camera.
        double posZ = (gscreenHeight / 2.0) + (vertHeight * gscreenHeight);

        // Horizontal distance from the camera to the floor for the current row.
        // 0.5 is the z position exactly in the middle between floor and ceiling.
        double rowDistance = posZ / p;

        floorDist[y] = rowDistance;
    }

    // for(int y = 0; y < gscreenHeight; y++) //define a height table for floor and ceiling calculations later
    // {
    //     floorDist[y] = (gscreenHeight*vFOV+(2*vertHeight*gscreenHeight))/ ((2.0*(y-vertLook) - (gscreenHeight)));
    //     ceilDist[y] = (gscreenHeight*vFOV-(2*vertHeight*gscreenHeight))/ ((2.0*(y+vertLook) - (gscreenHeight)));
    // }
}

void drawWorldGeoFlat(double* wallDist, int* side, int* mapX, int* mapY)
{
    //draw sky
    SDL_SetRenderDrawColor(gRenderer, 0x7f, 0xaa, 0xff, 0xff);
    SDL_RenderFillRect(gRenderer, &gskyDestRect);
    //draw floor
    SDL_SetRenderDrawColor(gRenderer, 0x7f, 0x7f, 0x7f, 0xff);
    SDL_RenderFillRect(gRenderer, &gfloorRect);
    int lineHeight, drawStart, drawEnd;
    SDL_Rect wallRect;
    for (int x = 0; x < gscreenWidth; x++)
        {
        lineHeight = (int)(gscreenHeight * vFOV / wallDist[x]);
        //calculate lowest and highest pixel to fill in current stripe
        drawStart = -lineHeight / 2 + gscreenHeight / 2;
        drawEnd = lineHeight / 2 + gscreenHeight / 2;
        //choose wall color
        SDL_Color color;
        switch (leveldata[mapX[x]][mapY[x]].block_id)
        {
        case 1:
            color = cBlue;
            break;
        case 2:
            color = cGreen;
            break;
        case 3:
            color = cRed;
            break;
        case 4:
            color = cWhite;
            break;
        default:
            color = cYellow;
            break;
        }
        //give x and y sides different brightness
        if (side[x] == 1)
        {
            color.r = color.r >> 1;
            color.g = color.g >> 1;
            color.b = color.b >> 1;
            color.a = color.a  >> 1;
        }

        //calculate lowest and highest pixel to fill in current stripe
        drawEnd = (lineHeight / 2) + (gscreenHeight / 2) + ((vertHeight*gscreenHeight) / wallDist[x]) + vertLook;
        drawStart = drawEnd - lineHeight;
        if (drawStart < 0)
            drawStart = 0;
        if (drawEnd >= gscreenHeight)
            drawEnd = gscreenHeight - 1;

        //render vertical lines for raycast based on calculations

        
        
        wallRect.x = x;
        wallRect.y = drawStart;
        wallRect.w = 1;
        wallRect.h = drawEnd - drawStart;
        SDL_SetRenderDrawColor(gRenderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(gRenderer, &wallRect);
    }
}

void drawWorldGeoTex(double* wallDist, int* side, int* mapX, int* mapY)
{
    if(ceilingOn == false)
    {
        drawSkyBox();
    }
    drawFloor();
    
    double cameraX, rayDirX, rayDirY, wallX, brightness;
    int lineHeight, texX;
    int drawStart[gscreenWidth], drawEnd[gscreenWidth];
    int currTexWidth, currTexHeight;
    
    for (int x = 0; x < gscreenWidth; x++)
    {
        cameraX = 2 * x / double(gscreenWidth) - 1; //x-coordinate in camera space, or along the x of the camera plane itself
        rayDirX = dirX + planeX * cameraX; //the XY coord where the vector of this ray crosses the camera plane
        rayDirY = dirY + planeY * cameraX;
        //Calculate height of line to draw on screen
        lineHeight = (int)(gscreenHeight * vFOV / wallDist[x]);
        int currentWall = 0;
        if (side[x] == 1 && rayDirY > 0) //NORTH WALL
            currentWall = NORTH;
        else if (side[x] == 1 && rayDirY < 0) //SOUTH WALL
            currentWall = SOUTH;
        else if (side[x] == 0 && rayDirX < 0) //EAST WALL
            currentWall = EAST;
        else //WEST WALL??
            currentWall = WEST;

        if(leveldata[mapX[x]][mapY[x]].wallTex[currentWall] < totalWallTextures)
        {
            gcurrTex = gwallTex[leveldata[mapX[x]][mapY[x]].wallTex[currentWall]];
        }
        else
        {
            gcurrTex = gwallTex[0];
        }
        SDL_QueryTexture(gcurrTex, NULL, NULL, &currTexWidth, &currTexHeight);        

        //calculate value of wallX
        if (side[x] == 0)
            wallX = posY + wallDist[x] * rayDirY; //if we hit a NS wall, use y pos, + perpendicular value * y component of vector to get total y offset
        else
            wallX = posX + wallDist[x] * rayDirX; //as above, but x value for EW walls
        wallX -= floor((wallX));                   //subtract away the digits to the left of the decimal point, leaving only the fractional value across the single wall


        wallX += 1.0 - leveldata[mapX[x]][mapY[x]].timer;

        //x coordinate on the texture
        texX = int(wallX * double(currTexWidth)); //determine exact value across the wall texture in pixels
        if (side[x] == 0 && rayDirX < 0)
            texX = currTexWidth - texX - 1; //horizontally flip textures so they're drawn properly depending on the side of the cube they're on
        if (side[x] == 1 && rayDirY > 0)
            texX = currTexWidth - texX - 1;

        //calculate lowest and highest pixel to fill in current stripe
        drawStart[x] = -lineHeight / 2 + (gscreenHeight / 2) + ((vertHeight*gscreenHeight) / wallDist[x]) + vertLook;
        drawEnd[x] = lineHeight / 2 + (gscreenHeight / 2) + ((vertHeight*gscreenHeight) / wallDist[x]) + vertLook;

        // set up the rectangle to sample the texture for the wall
        SDL_Rect line = {x, drawStart[x], 1, drawEnd[x] - drawStart[x]};
        SDL_Rect sample = {texX, 0, 1, currTexHeight};

        //use color mod to darken the wall texture
        //255 = no color mod, lower values mean darker
        //currently setup so that NS walls are full brightness, and EW walls are darkened
        if(side[x] == 0)
            brightness = 255.0;
        else
            brightness = 127.0;

        SDL_SetTextureColorMod(gcurrTex, brightness, brightness, brightness);
        renderTexture(gcurrTex, gRenderer, line, &sample);   
    }
    //render distance fog on top of floor/ceiling textures
    //
    //THIS IS SLOOOOWWWWW
    //
    //TODO: speed up fog map
    if(fogOn)
    {
        generatefogMask(gfogTex, drawStart, drawEnd, wallDist);
        SDL_SetTextureBlendMode(gfogTex, SDL_BLENDMODE_BLEND);
        SDL_RenderCopy(gRenderer, gfogTex, NULL, NULL);
    }
}

void drawFloor() //affine mapping accross entire screen, has artifacts
{
    //create some pointers to later access the pixels in the floor and buffer textures
    void *floorBufferPixels, *floorTexPixels, *ceilTexPixels;
    int floorBufferPitch, floorTexPitch, ceilTexPitch;//, floorTexX, floorTexY;// lineHeight;
    int floorTexWidth, floorTexHeight, ceilTexWidth, ceilTexHeight;

    //lock floor textures and a screen buffer texture for read/write operations
    SDL_LockTexture(gfloorTex, NULL, &floorTexPixels, &floorTexPitch); 
    SDL_LockTexture(gceilTex, NULL, &ceilTexPixels, &ceilTexPitch);
    SDL_LockTexture(gfloorBuffer, NULL, &floorBufferPixels, &floorBufferPitch);
    Uint32 *bufferPixels = (Uint32 *)floorBufferPixels; //access pixel data as a bunch of Uint32s. add handling for 24 bit possibility?
    Uint32 *ufloorTexPix = (Uint32 *)floorTexPixels;
    Uint32 *uceilTexPix = (Uint32 *)ceilTexPixels;


    // double floorXWall, floorYWall, currentFloorX, currentFloorY, weight, cameraX, rayDirX, rayDirY, wallX;
    // double distPlayer;

    //get attributes of the floor source texture
    SDL_QueryTexture(gfloorTex, NULL, NULL, &floorTexWidth, &floorTexHeight);
    SDL_QueryTexture(gceilTex, NULL, NULL, &ceilTexWidth, &ceilTexHeight);
    // rayDir for leftmost ray (x = 0) and rightmost ray (x = w)
    float rayDirX0 = dirX - planeX;
    float rayDirY0 = dirY - planeY;
    float rayDirX1 = dirX + planeX;
    float rayDirY1 = dirY + planeY;

    if(ceilingOn)
    {
        for(int y = 0; y < ((gscreenHeight / 2) + vertLook); y++)
        {
            // // Current y position compared to the center of the screen (the horizon)
            // int p = y - ((gscreenHeight / 2) + vertLook);

            // // Vertical position of the camera.
            // float posZ = (gscreenHeight / 2.0) - (vertHeight * gscreenHeight);

            // // Horizontal distance from the camera to the floor for the current row.
            // // 0.5 is the z position exactly in the middle between floor and ceiling.
            // float rowDistance = -posZ / p;

            // calculate the real world step vector we have to add for each x (parallel to camera plane)
            // adding step by step avoids multiplications with a weight in the inner loop
            // float floorStepX = rowDistance * (rayDirX1 - rayDirX0) / gscreenWidth;
            // float floorStepY = rowDistance * (rayDirY1 - rayDirY0) / gscreenWidth;
            float floorStepX = floorDist[y] * (rayDirX1 - rayDirX0) / gscreenWidth;
            float floorStepY = floorDist[y] * (rayDirY1 - rayDirY0) / gscreenWidth;


            // real world coordinates of the leftmost column. This will be updated as we step to the right.
            // float floorX = posX + rowDistance * rayDirX0;
            // float floorY = posY + rowDistance * rayDirY0;
            float floorX = posX + floorDist[y] * rayDirX0;
            float floorY = posY + floorDist[y] * rayDirY0;


            for(int x = 0; x < gscreenWidth; ++x)
            {
                // the cell coord is simply got from the integer parts of floorX and floorY
                int cellX = (int)(floorX);
                int cellY = (int)(floorY);

                // get the texture coordinate from the fractional part
                int tx = (int)(floorTexWidth * (floorX - cellX)) & (floorTexWidth - 1);
                int ty = (int)(floorTexHeight * (floorY - cellY)) & (floorTexHeight - 1);

                floorX += floorStepX;
                floorY += floorStepY;

                Uint32 color;
                //ceiling (symmetrical, at screenHeight - y - 1 instead of y)
                color = uceilTexPix[ceilTexWidth * ty + tx];
                //color = (color >> 1) & 8355711; // make a bit darker
                bufferPixels[gscreenWidth * y + x] = color;
            }        

        }
    }    
    for(int y = ((gscreenHeight / 2) + vertLook); y < gscreenHeight; y++)
    {
            // // Current y position compared to the center of the screen (the horizon)
            // int p = y - ((gscreenHeight / 2) + vertLook);
            // // Vertical position of the camera.
            // float posZ = (gscreenHeight / 2.0) + (vertHeight * gscreenHeight);

            // // Horizontal distance from the camera to the floor for the current row.
            // // 0.5 is the z position exactly in the middle between floor and ceiling.
            // float rowDistance = posZ / p;

            // calculate the real world step vector we have to add for each x (parallel to camera plane)
            // adding step by step avoids multiplications with a weight in the inner loop
            // float floorStepX = rowDistance * (rayDirX1 - rayDirX0) / gscreenWidth;
            // float floorStepY = rowDistance * (rayDirY1 - rayDirY0) / gscreenWidth;

            float floorStepX = floorDist[y] * (rayDirX1 - rayDirX0) / gscreenWidth;
            float floorStepY = floorDist[y] * (rayDirY1 - rayDirY0) / gscreenWidth;

            // real world coordinates of the leftmost column. This will be updated as we step to the right.
            // float floorX = posX + rowDistance * rayDirX0;
            // float floorY = posY + rowDistance * rayDirY0;

            float floorX = posX + floorDist[y] * rayDirX0;
            float floorY = posY + floorDist[y] * rayDirY0;

            for(int x = 0; x < gscreenWidth; ++x)
            {
                // the cell coord is simply got from the integer parts of floorX and floorY
                int cellX = (int)(floorX);
                int cellY = (int)(floorY);

                // get the texture coordinate from the fractional part
                int tx = (int)(floorTexWidth * (floorX - cellX)) & (floorTexWidth - 1);
                int ty = (int)(floorTexHeight * (floorY - cellY)) & (floorTexHeight - 1);

                floorX += floorStepX;
                floorY += floorStepY;

                Uint32 color;

                // floor
                color = ufloorTexPix[floorTexWidth * ty + tx];
                //color = (color >> 1) & 8355711; // make a bit darker
                bufferPixels[gscreenWidth * y + x] = color;
            }     
    }
    //Render floor by unlocking floor textures and buffer, and copying the entire buffer onto the render target in one go
    //Unlock texture
    SDL_UnlockTexture(gfloorTex);
    SDL_UnlockTexture(gceilTex);
    SDL_UnlockTexture(gfloorBuffer);
    SDL_SetTextureBlendMode(gfloorBuffer, SDL_BLENDMODE_BLEND);
    if(ceilingOn)
        SDL_RenderCopy(gRenderer, gfloorBuffer, NULL, NULL);
    else
        SDL_RenderCopy(gRenderer, gfloorBuffer, &gfloorRect, &gfloorRect);
}


//draw floor using vertical stripes, much slower but fewer artifacts
//this is the only reason to keep the floor and ceiling distance buffers
void drawFloor(double* wallDist, int* drawStart, int* drawEnd, int* side, int* mapX, int* mapY)
{
    //create some pointers to later access the pixels in the floor and buffer textures
    void *floorBufferPixels, *floorTexPixels, *ceilTexPixels;
    int floorBufferPitch, floorTexPitch, ceilTexPitch, floorTexX, floorTexY;
    int floorTexWidth, floorTexHeight, ceilTexWidth, ceilTexHeight;

    //lock floor textures and a screen buffer texture for read/write operations

    SDL_LockTexture(gfloorTex, NULL, &floorTexPixels, &floorTexPitch); 
    SDL_LockTexture(gceilTex, NULL, &ceilTexPixels, &ceilTexPitch);
    SDL_LockTexture(gfloorBuffer, NULL, &floorBufferPixels, &floorBufferPitch);

    Uint32 *bufferPixels = (Uint32 *)floorBufferPixels; //access pixel data as a bunch of Uint32s. add handling for 24 bit possibility?
    Uint32 *ufloorTexPix = (Uint32 *)floorTexPixels;
    Uint32 *uceilTexPix = (Uint32 *)ceilTexPixels;


    double floorXWall, floorYWall, currentFloorX, currentFloorY, weight, cameraX, rayDirX, rayDirY, wallX;
    double distPlayer;

    for (int x = 0; x < gscreenWidth; x++)
    {
        cameraX = 2 * x / double(gscreenWidth) - 1; //x-coordinate in camera space, or along the x of the camera plane itself
        rayDirX = dirX + planeX * cameraX; //the XY coord where the vector of this ray crosses the camera plane
        rayDirY = dirY + planeY * cameraX;
        //calculate value of wallX
        if (side[x] == 0)
            wallX = posY + wallDist[x] * rayDirY; //if we hit a NS wall, use y pos, + perpendicular value * y component of vector to get total y offset
        else
            wallX = posX + wallDist[x] * rayDirX; //as above, but x value for EW walls
        wallX -= floor((wallX)); 

        //4 different wall directions possible
        if (side[x] == 0 && rayDirX > 0)
        {
            floorXWall = mapX[x];
            floorYWall = mapY[x] + wallX;
        }
        else if (side[x] == 0 && rayDirX < 0)
        {
            floorXWall = mapX[x] + 1.0;
            floorYWall = mapY[x] + wallX;
        }
        else if (side[x] == 1 && rayDirY > 0)
        {
            floorXWall = mapX[x] + wallX;
            floorYWall = mapY[x];
        }
        else
        {
            floorXWall = mapX[x] + wallX;
            floorYWall = mapY[x] + 1.0;
        }

        distPlayer = viewTrip;
        // alters the pixel width of the floor that's used to show depth
        // distPlayer values other than 0.0 will warp the floor/ceiling curvature. (+) values will curve "up" towards the player, and (-) values dropping downward

        if (drawEnd[x] > gscreenHeight || drawEnd[x] < 0) //becomes < 0 when the integer overflows
            drawEnd[x] = gscreenHeight; 


        //get attributes of the floor source texture
        SDL_QueryTexture(gfloorTex, NULL, NULL, &floorTexWidth, &floorTexHeight); //get height and width of floor tile texture

        //draw floor in vertical stripe from bottom of wall to bottom of screen
        for (int y = drawEnd[x]; y < gscreenHeight; y++)
        {
            //calculate pixel based on perspective
            weight = (floorDist[y] - distPlayer) / (wallDist[x] - distPlayer);
            currentFloorX = weight * floorXWall + (1.0 - weight) * posX;
            currentFloorY = weight * floorYWall + (1.0 - weight) * posY;

            //select that pixel from the source texture
            floorTexX = int(currentFloorX * floorTexWidth) % floorTexWidth;
            floorTexY = int(currentFloorY * floorTexHeight) % floorTexHeight;

            //set destination pixel to the selected pixel from the source
            bufferPixels[y * gscreenWidth + x] = ufloorTexPix[floorTexWidth * floorTexY + floorTexX];
        }
        
        if(ceilingOn)
        {
            SDL_QueryTexture(gceilTex, NULL, NULL, &ceilTexWidth, &ceilTexHeight); //get ceiling tile texture width and height
            for (int y = gscreenHeight - drawStart[x]; y < gscreenHeight; y++)
            {
                
                //weight = (ceilDist[y] - distPlayer) / (wallDist[x] - distPlayer);
                weight = (floorDist[gscreenHeight - y - 1] - distPlayer) / (wallDist[x] - distPlayer);
                currentFloorX = weight * floorXWall + (1.0 - weight) * posX;
                currentFloorY = weight * floorYWall + (1.0 - weight) * posY;
                floorTexX = int(currentFloorX * ceilTexWidth) % ceilTexWidth;
                floorTexY = int(currentFloorY * ceilTexHeight) % ceilTexHeight;

                bufferPixels[(gscreenHeight - y - 1) * gscreenWidth + x] = uceilTexPix[ceilTexWidth * floorTexY + floorTexX];
            }
        }
    }

    //Render floor by unlocking floor textures and buffer, and copying the entire buffer onto the render target in one go
    //Unlock texture
    SDL_UnlockTexture(gfloorTex);
    SDL_UnlockTexture(gceilTex);
    SDL_UnlockTexture(gfloorBuffer);
    SDL_SetTextureBlendMode(gfloorBuffer, SDL_BLENDMODE_BLEND);
    if(ceilingOn)
        SDL_RenderCopy(gRenderer, gfloorBuffer, NULL, NULL);
    else
        SDL_RenderCopy(gRenderer, gfloorBuffer, &gfloorRect, &gfloorRect);
    
}

void drawSkyBox()
{
    //Here I'm creating a sky box and rotating it according to player's viewing angle
    //trying to match drawn sky segment to FOV
    
    double angle = atan2(dirY, dirX) * radToDeg; // gets view dir in degrees

    if (angle < 0)
        angle += 360;
    
    int tw, th;
    SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th); //get texture width and height
    angle *= (double)tw/360.0; //converts 360 degress to texture width

    angle = round(angle); //no fractional pixels allowed
        if (angle > tw)
            angle -= tw;
        if (angle < 0)
            angle += tw;
    gskySrcRect.x = angle;
    if(tw - gskySrcRect.x < gskySrcRect.w) //reached end of texture and need to draw sky in two parts
    {
        //draw first part, wrap to 0, draw rest
        SDL_Rect tempSrc = gskySrcRect;
        SDL_Rect tempDest = gskyDestRect;
        tempSrc.w = tw - tempSrc.x;
        tempDest.w = (int)((double)tempSrc.w * ((double)gskyDestRect.w / (double)gskySrcRect.w));
        SDL_RenderCopy(gRenderer, gskyTex, &tempSrc, &tempDest);

        tempSrc.x = 0;
        tempSrc.w = gskySrcRect.w - tempSrc.w;
        tempDest.x += tempDest.w;
        tempDest.w = gskyDestRect.w - tempDest.w;
        SDL_RenderCopy(gRenderer, gskyTex, &tempSrc, &tempDest);

    }
    else //safe to draw entire sky rect at once
        SDL_RenderCopy(gRenderer, gskyTex, &gskySrcRect, &gskyDestRect); //now paste our chunk of sky onto the renderer
}

void drawSprites(double* wallDist)
{

    //TODO do a better job of sorting sprites
    for(std::size_t i = 0; i != allSprites.size(); ++i)
    {
        spriteDistances[i] = ((posX - allSprites[i].worldX)*(posX - allSprites[i].worldX)+(posY - allSprites[i].worldY)*(posY - allSprites[i].worldY));
        spriteOrder[i] = i;
    }
    //void sortSprites(int* order, double* dist, int amount)
    int amount = allSprites.size();
    std::vector<std::pair<double, int>> sortSpritePair(amount);
    for(int i = 0; i < amount; i++) {
        sortSpritePair[i].first = spriteDistances[i];
        sortSpritePair[i].second = spriteOrder[i];
    }
    std::sort(sortSpritePair.begin(), sortSpritePair.end());
    // restore in reverse order to go from farthest to nearest
    for(int i = 0; i < amount; i++) {
        spriteDistances[i] = sortSpritePair[amount - i - 1].first;
        spriteOrder[i] = sortSpritePair[amount - i - 1].second;
    }

    double brightness;

    double invDet = 1.0 / (planeX * dirY - dirX * planeY); //required for correct matrix multiplication

    for(auto i = 0; i < amount; ++i)
    {
        double spriteX = allSprites[spriteOrder[i]].worldX - posX;
        double spriteY = allSprites[spriteOrder[i]].worldY - posY;

        //transform sprite with the inverse camera matrix
        // [ planeX   dirX ] -1                                       [ dirY      -dirX ]
        // [               ]       =  1/(planeX*dirY-dirX*planeY) *   [                 ]
        // [ planeY   dirY ]                                          [ -planeY  planeX ]

        double transformY = invDet * (-planeY * spriteX + planeX * spriteY); //this is actually the depth inside the screen, that what Z is in 3D
        if(transformY > 0) //transformY values < 0 are behind player
        {
            double transformX = invDet * (dirY * spriteX - dirX * spriteY);
            int spriteScreenX = int((gscreenWidth / 2) * (1 + transformX / transformY));

            int spriteHeight = abs(int(gscreenHeight / (transformY))); //using 'transformY' instead of the real distance prevents fisheye


            //calculate lowest and highest pixel to fill in current stripe   
            int drawEndY = spriteHeight / 2 + gscreenHeight / 2 + (vertHeight * abs(int(gscreenHeight / (transformY)))) + vertLook;

            spriteHeight *= allSprites[spriteOrder[i]].height;

            int drawStartY = drawEndY - spriteHeight;

            //calculate width of the sprite
            int spriteWidth = abs( int (gscreenHeight / (transformY))) * allSprites[spriteOrder[i]].width;        
            int drawEndX = spriteWidth / 2 + spriteScreenX;
            int drawStartX = drawEndX - spriteWidth;

            SDL_Rect clip, dest;
            clip.x = drawStartX;
            clip.y = drawStartY;
            clip.w = drawEndX - drawStartX;
            clip.h = drawEndY - drawStartY;
            dest = clip;

            if(drawStartX < 0) drawStartX = 0;
            if(drawEndX >= gscreenWidth) drawEndX = gscreenWidth - 1;
            clip.x = drawStartX;
            clip.w = drawEndX - drawStartX;

            for(auto testX = drawStartX; testX <= drawEndX+1; testX++)
            {
                clip.x = testX;
                if(transformY < wallDist[testX])
                    break;
            }
            for(auto testX = drawEndX; testX >= clip.x; --testX)
            {
                clip.w = testX-clip.x;
                if(transformY < wallDist[testX])
                    break;
            }


            if(clip.y < 0) clip.y = 0;
            if(clip.y+clip.h >= gscreenHeight) clip.h = gscreenHeight - clip.y - 1;

            if(clip.x+clip.w >= 0 && clip.x < gscreenWidth)
            {
                SDL_RenderSetClipRect(gRenderer, &clip);
                SDL_SetTextureBlendMode(pickupTex[allSprites[spriteOrder[i]].texID], SDL_BLENDMODE_BLEND);
                SDL_RenderCopy(gRenderer, pickupTex[allSprites[spriteOrder[i]].texID], &allSprites[spriteOrder[i]].image, &dest);

                if(fogOn && (!debugColors))
                {
                    int shadowX = std::min(std::max(dest.x + (dest.w/2),0),gscreenWidth-1);        
                    transformY *= (90.0/hFOV);     
                    brightness = std::min(1.0,std::max(worldFog,std::min(playerFog,playerFog/((fogMultiplier* brightSin[shadowX]) * transformY * transformY))));  
                    brightness = std::max(std::min(brightness, playerFog),worldFog);

                    fogColor.a = (Uint8)255.0*(1.0-std::min(1.0,std::max(worldFog, brightness)));
                    SDL_SetTextureBlendMode(maskTex[allSprites[spriteOrder[i]].texID], SDL_BLENDMODE_BLEND);
                    SDL_SetTextureColorMod(maskTex[allSprites[spriteOrder[i]].texID], fogColor.r, fogColor.g, fogColor.b);
                    SDL_SetTextureAlphaMod(maskTex[allSprites[spriteOrder[i]].texID], fogColor.a);
                    SDL_RenderCopy(gRenderer, maskTex[allSprites[spriteOrder[i]].texID], &allSprites[spriteOrder[i]].image, &dest);
                }

                SDL_RenderSetClipRect(gRenderer, NULL);
            }

        }
    }




}

void close()
{
    //destroy renderer
    SDL_DestroyTexture(gskyTex);
    //SDL_DestroyTexture(gDoorTex);
    SDL_DestroyTexture(gwallTex[0]);
    SDL_DestroyTexture(gwallTex[1]);
    SDL_DestroyTexture(gfloorTex);
    SDL_DestroyTexture(gceilTex);
    SDL_DestroyTexture(weaponTex);
    gskyTex = NULL;
    //gDoorTex = NULL;
    gwallTex[0] = NULL;
    gwallTex[1] = NULL;
    gfloorTex = NULL;
    gceilTex = NULL;
    gcurrTex = NULL;
    weaponTex = NULL;
    SDL_DestroyRenderer(gRenderer);
    gRenderer = NULL;
    SDL_DestroyWindow(gwindow);
    gwindow = NULL;
    SDL_Quit();
}

void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, SDL_Rect dst, SDL_Rect *clip = nullptr)
{
    // draw the clipped texture to the destination rectangle
    SDL_RenderCopy(ren, tex, clip, &dst);
}
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y, SDL_Rect *clip = nullptr)
{
    //draw entire texture at given x y
    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    SDL_QueryTexture(tex, NULL, NULL, &dst.w, &dst.h);
    renderTexture(tex, ren, dst, clip);
}

std::string getProjectPath(const std::string &subDir = "")
{
    //We need to choose the path separator properly based on which
    //platform we're running on, since Windows uses a different
    //separator than most systems
#ifdef _WIN32
    const char PATH_SEP = '\\';
#else
    const char PATH_SEP = '/';
#endif
    //This will hold the base resource path: Lessons/res/
    //We give it static lifetime so that we'll only need to call
    //SDL_GetBasePath once to get the executable path
    static std::string baseRes;
    if (baseRes.empty())
    {
        //SDL_GetBasePath will return NULL if something went wrong in retrieving the path
        char *basePath = SDL_GetBasePath();
        if (basePath)
        {
            baseRes = basePath;
            SDL_free(basePath);
        }
        else
        {
            printf("Error getting base resource path: %s\n", SDL_GetError());
            printf("Error: Failed to get application resource base path. \n %s", SDL_GetError());
            return "";
        }
    }
    //If we want a specific subdirectory path in the resource directory
    //append it to the base path. This would be something like Lessons/res/Lesson0
    return subDir.empty() ? baseRes : baseRes + subDir + PATH_SEP;
}

SDL_Texture *loadImage(std::string path)
{
    static std::string projectPath = getProjectPath();

    SDL_Surface *bmp = SDL_LoadBMP((projectPath + path).c_str());
    SDL_Texture *tex = NULL;
    if (bmp == nullptr)
    {
        printf("SDL_LoadBMP Error: %s\n", SDL_GetError());
    }
    else
    {
        bmp = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA32, 0);
        tex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, bmp->w, bmp->h);

        //Lock texture for manipulation
        void *mPixels;
        int mPitch;
        SDL_LockTexture(tex, NULL, &mPixels, &mPitch);
        //Copy loaded/formatted surface pixels
        memcpy(mPixels, bmp->pixels, mPitch * bmp->h);
        //Unlock texture to update
        SDL_UnlockTexture(tex);
        mPixels = NULL;

        SDL_FreeSurface(bmp);
        if (tex == nullptr)
        {
            printf("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        }
    }

    return tex;
}

SDL_Texture *loadImageColorKey(std::string path, SDL_Color transparent)
{

    static std::string projectPath = getProjectPath();

    SDL_Surface *bmp = SDL_LoadBMP((projectPath + path).c_str());
    SDL_Texture *tex = NULL;
    if (bmp == nullptr)
    {
        printf("SDL_LoadBMP Error: %s\n", SDL_GetError());
    }
    else
    {
        bmp = SDL_ConvertSurfaceFormat(bmp, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, transparent.r, transparent.g, transparent.b));
        tex = SDL_CreateTextureFromSurface(gRenderer,bmp);

        SDL_FreeSurface(bmp);
        if (tex == nullptr)
        {
            printf("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        }
    }

    return tex;
}

void generatefogMask(SDL_Texture* tex, int* drawStart, int* drawEnd, double* wallDist)
{
    int texWidth;
    int texHeight;
    void* texPix = NULL;
    int pitch;//, lineHeight;
    double brightness = 0;
    Uint32 format;
    SDL_QueryTexture(tex, &format, NULL, &texWidth, &texHeight);
    SDL_LockTexture(tex, NULL, &texPix, &pitch);
    Uint32* pixels = (Uint32*)texPix;
    SDL_PixelFormat *mappingFormat = SDL_AllocFormat(format);
    for(int x = 0; x < texWidth; x++)
    {
        int start = std::max(drawStart[x],0);
        int end = std::min(drawEnd[x], texHeight);


        for(int y = 0; y < start; y++)
        {
            double dist = floorDist[y] * floorDist[y] * (90.0/hFOV) * (90.0/hFOV);
            brightness = std::min(1.0,std::max(worldFog,std::min(playerFog,playerFog/((fogMultiplier*  brightSin[x]) * dist))));
            
            fogColor.a = (Uint8)255.0*(1.0-std::min(1.0,std::max(worldFog, brightness)));
            pixels[y * texWidth + x] = SDL_MapRGBA(mappingFormat, fogColor.r, fogColor.g, fogColor.b, fogColor.a);
        }
        
        double tempdist = wallDist[x] * wallDist[x] * (90.0/hFOV) * (90.0/hFOV);
        
        brightness = std::min(1.0,std::max(worldFog,std::min(playerFog,playerFog/((fogMultiplier* brightSin[x]) * tempdist))));
        fogColor.a = (Uint8)255.0*(1.0-std::min(1.0,std::max(worldFog, brightness)));
        for(int y = start; y < end; y++)
        {
            pixels[y * texWidth + x] = SDL_MapRGBA(mappingFormat, fogColor.r, fogColor.g, fogColor.b, fogColor.a);
        }
        for(int y = end; y < texHeight; y++)
        {
            double dist = floorDist[y] * floorDist[y] * (90.0/hFOV) * (90.0/hFOV);
            brightness = std::min(1.0,std::max(worldFog,std::min(playerFog,playerFog/((fogMultiplier* brightSin[x]) * dist))));
            fogColor.a = (Uint8)255.0*(1.0-std::min(1.0,std::max(worldFog, brightness)));
            pixels[y * texWidth + x] = SDL_MapRGBA(mappingFormat, fogColor.r, fogColor.g, fogColor.b, fogColor.a);
        }
        
    }
    SDL_FreeFormat(mappingFormat);
    SDL_UnlockTexture(tex);

    return;
}

void drawHud()
{
    drawWeap();
    if(mapOn)
        drawMiniMap();
    return;
}
void drawWeap()
{
    renderTexture(weaponTex, gRenderer, weaponDestRect, &weaponTexRect);
    return;
}

void drawMiniMap()
{

    SDL_SetRenderDrawColor(gRenderer, cBlack.r, cBlack.g, cBlack.b, cBlack.a);
    SDL_RenderFillRect(gRenderer, &miniMapRect);
    for(int y = 0; y < mapHeight; y++)
    {
        for(int x = 0; x < mapWidth; x++)
        {
            if(leveldata[x][y].visible)
            {
                switch(leveldata[x][y].block_id)
                {
                    case(1):
                        SDL_SetRenderDrawColor(gRenderer, cBlue.r, cBlue.g, cBlue.b, cBlue.a);
                        break;
                    case(2):
                        SDL_SetRenderDrawColor(gRenderer, cGreen.r, cGreen.g, cGreen.b, cGreen.a);
                        break;
                    case(3):
                        SDL_SetRenderDrawColor(gRenderer, cRed.r, cRed.g, cRed.b, cRed.a);
                        break;
                    default:
                        SDL_SetRenderDrawColor(gRenderer, cBlack.r, cBlack.g, cBlack.b, cBlack.a);
                        break;
                }
                miniMapDot.x = miniMapRect.x + 2*x;
                miniMapDot.y = miniMapRect.y + 2*y;
                SDL_RenderDrawRect(gRenderer, &miniMapDot);
            }
        }
    }
    SDL_SetRenderDrawColor(gRenderer, cYellow.r, cYellow.g, cYellow.b, cYellow.a);
    miniMapDot.x = miniMapRect.x + 2*(int)posX;
    miniMapDot.y = miniMapRect.y + 2*(int)posY;
    SDL_RenderDrawRect(gRenderer, &miniMapDot);
    SDL_SetRenderDrawColor(gRenderer, cCyan.r, cCyan.g, cCyan.b, cCyan.a);
    SDL_RenderDrawLine(gRenderer, miniMapRect.x + 2*(int)posX, miniMapRect.y + 2*(int)posY, miniMapRect.x + 2*(int)(blockLeftX), miniMapRect.y + 2*(int)(blockLeftY));
    SDL_RenderDrawLine(gRenderer, miniMapRect.x + 2*(int)posX, miniMapRect.y + 2*(int)posY, miniMapRect.x + 2*(int)(blockRightX), miniMapRect.y + 2*(int)(blockRightY));
    SDL_RenderDrawLine(gRenderer, miniMapRect.x + 2*(int)posX, miniMapRect.y + 2*(int)posY, miniMapRect.x + 2*(int)(blockAheadX), miniMapRect.y + 2*(int)(blockAheadY));
    return;
}

void newlevel(bool warpView)
{
        enableInput = false; //shouldn't matter but just in case, disable player input during new level load
        if(warpView)
        {
            for(int i = 0; i < 50; i++)
            {
                viewTrip -= 0.2; //make floor and ceiling appear to stretch / fall away. acid trip effect. just for fun

                updateScreen();
                SDL_Delay(20);
            }
        }

        //randomly load a new level from maps directory, named map0.txt to map19.txt
        //eventually this might get replced with the random level gen function or something else
        std::stringstream levelFileName;
        srand(time(0));
        std::string thisMap;
        thisMap += "map";
        thisMap += std::to_string(rand() % 20);
        levelFileName << getProjectPath("resources") << PATH_SYM << "maps" << PATH_SYM << thisMap << ".txt";
        ceilingOn = rand()%2;
        printf("Map: %s\n", thisMap.c_str());
        loadLevel(levelFileName.str());

        //reset all the camera stuff
        //dirX = std::tan((hFOV*degToRad)/2);
        //dirX = std::sqrt(dirX*dirX+dirY*dirY); //preserve hfov. not sure why the other way isn't working
        double oldFOV = hFOV;
        changeFOV(false, 90);
        dirX = 1;
        dirY = 0;
        changeFOV(false, oldFOV);        
        planeX = 0;
        planeY = 1;
        vertLook = 0;
        vertHeight = 0.1;
        viewTrip = 0;
        initAllSprites();
        calcFloorDist();

        int tw, th;
        SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th);
        gskySrcRect.y = (int)std::round(((double)th/2.0 - (double)gskySrcRect.h/2.0) - ((double)vertLook * ((double)gskySrcRect.h/(double)gskyDestRect.h)));
        if (gskySrcRect.y < 0)
            gskySrcRect.y = 0;
        gfloorRect.y = gscreenHeight / 2 + vertLook;
        gfloorRect.h = gscreenHeight - gfloorRect.y;

        miniMapRect = { gscreenWidth - (mapWidth*2) - gscreenWidth/16,
                        gscreenHeight / 16,
                        mapWidth*2,
                        mapHeight*2 };


        enableInput = true;
}

void loadLevel(std::string path)
{
    

    std::ifstream mapFile(path);
    int a;
    mapFile >> a;
    posX = a + 0.5;
    mapFile >> a;
    posY = a + 0.5;
    mapFile >> a;
    mapWidth = a;
    mapFile >> a;
    mapHeight = a;

    leveldata.resize(mapWidth);
    for (int i = 0; i < mapWidth; i++)
    {
        leveldata[i].resize(mapHeight);
    }

    for (int y = 0; y < mapHeight; y++)
    {
        for (int x = 0; x < mapWidth; x++)
        {
            mapFile >> a;
            changeBlock(&leveldata.at(x).at(y), a);
        }
    }

    mapFile.close();
}

void changeFOV(bool rel, double newFOV)
{
    if(rel)
    {
        if((hFOV + newFOV >= 180) || (hFOV + newFOV < 45))
            return;
        if(std::tan((hFOV+newFOV) * degToRad) != 0)
        {
            double mult = std::tan((hFOV * degToRad)/2) / std::tan(((hFOV+newFOV) * degToRad)/2);
            hFOV += newFOV;
            dirX *= mult;
            dirY *= mult;
        }
    }
    else
    {
        if(std::tan(newFOV * degToRad) != 0)
        {
            double mult = std::tan((hFOV * degToRad)/2) / std::tan(((newFOV) * degToRad)/2);
            hFOV = newFOV;
            dirX *= mult;
            dirY *= mult;
        }
    }
}

void resizeWindow(bool letterbox)
{
    int newWidth = 0, newHeight = 0;
    SDL_GetWindowSize(gwindow, &newWidth, &newHeight);
    SDL_RenderSetLogicalSize(gRenderer, gscreenWidth, gscreenHeight);
    SDL_RenderSetIntegerScale(gRenderer, SDL_bool(false));
    float limitingSide = std::min((float)newWidth / gscreenWidth, (float)newHeight / gscreenHeight);
    SDL_RenderSetScale(gRenderer, limitingSide, limitingSide);
}

bool handleInput()
{
    bool quit = false;
    int mouseXDist = 0, mouseYDist = 0;
    SDL_GetRelativeMouseState(&mouseXDist,&mouseYDist);

    const Uint8 *currentKeyStates = SDL_GetKeyboardState(NULL);

    //Event handler
    SDL_Event e;
    //Handle events on queue
    SDL_PumpEvents();
    while (SDL_PollEvent(&e) != 0)
    {
        //User requests to quit by pressing X button

        switch (e.type)
        {
            case(SDL_QUIT):
            {
                quit = true;
                break;
            }
            case(SDL_MOUSEBUTTONDOWN):
            {
                if (enableInput)
                {
                    if (e.button.button == SDL_BUTTON_RIGHT && e.button.type == SDL_MOUSEBUTTONDOWN)
                    {
                        if(blockAheadDist < 1)
                        {
                            switch (leveldata[blockAheadX][blockAheadY].block_id)
                            {
                            case BLOCK_WALL:
                                //standard wall;
                                break;
                            case BLOCK_PANEL:
                                //exit panel;
                                newlevel(false);
                                break;
                            case BLOCK_DOOR:
                                //door;
                                //changeBlock(&leveldata[blockAheadX][blockAheadY], 0);
                                leveldata[blockAheadX][blockAheadY].timerOn = true;
                                break;
                            default:
                                //likely, an error;
                                break;
                            }
                        }
                    }
                }
                break;
            }
            case(SDL_MOUSEWHEEL):  //if scroll mousewheel, change some engine attributes
            {
                if (e.wheel.y != 0)
                {
                    //insert, home, and pageup used to control lighting
                    if(currentKeyStates[SDL_SCANCODE_INSERT]) //fog multiplier
                    {
                        fogMultiplier *= 1 + (0.04 * e.wheel.y);
                    }
                    else if(currentKeyStates[SDL_SCANCODE_HOME]) //world minimum brightness
                    {
                        worldFog = std::min(1.0,std::max(0.0,worldFog + (0.01*e.wheel.y)));
                    }
                    else if(currentKeyStates[SDL_SCANCODE_PAGEUP]) //local player lightsource brightness
                    {
                        playerFog = std::min(1.0,std::max(0.0,playerFog + (0.01*e.wheel.y)));
                    }
                    //delete, end, pagedown used to control camera
                    else if(currentKeyStates[SDL_SCANCODE_DELETE]) //horizontal FOV
                    {
                        fogColor.r = std::min(255,std::max(0,fogColor.r+2*e.wheel.y));
                    }
                    else if (currentKeyStates[SDL_SCANCODE_END]) //mouse horizontal sensitivity
                    {
                        fogColor.g = std::min(255,std::max(0,fogColor.g+2*e.wheel.y)); 
                    }
                    else if (currentKeyStates[SDL_SCANCODE_PAGEDOWN]) //mouse vertical sensitivity
                    {
                        fogColor.b = std::min(255,std::max(0,fogColor.b+2*e.wheel.y));
                    }
                    else if (currentKeyStates[SDL_SCANCODE_8]) //mouse vertical sensitivity
                    {
                        changeFOV(true, e.wheel.y);
                    }
                    else if (currentKeyStates[SDL_SCANCODE_9]) //mouse vertical sensitivity
                    {
                        mouseSense = std::min(5.0,std::max(0.01,mouseSense+0.01*e.wheel.y));
                    }
                    else if (currentKeyStates[SDL_SCANCODE_0]) //mouse vertical sensitivity
                    {
                        mouseVertSense = std::min(5.0,std::max(0.01,mouseVertSense+0.01*e.wheel.y));
                    }
                    
                }                

                break;
            }
            case(SDL_KEYDOWN): //on key press
            {
                if (e.key.repeat == false)
                {
                    switch (e.key.keysym.sym)
                    {
                        case SDLK_SPACE:
                        {
                            if (enableInput)
                            {
                                if(blockAheadDist < 1)
                                {
                                    switch (leveldata[blockAheadX][blockAheadY].block_id)
                                    {
                                    case BLOCK_WALL:
                                        //standard wall;
                                        break;
                                    case BLOCK_PANEL:
                                        //exit panel;
                                        newlevel(false);
                                        break;
                                    case BLOCK_DOOR:
                                        //door;
                                        //changeBlock(&leveldata[blockAheadX][blockAheadY], 0);
                                        leveldata[blockAheadX][blockAheadY].timerOn = true;
                                        break;
                                    default:
                                        //likely, an error;
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case SDLK_F5:
                        {
                            letterboxOn = !(letterboxOn);
                            break;
                        }
                        case SDLK_F6:
                        {
                            mapOn = !(mapOn);
                            break;
                        }
                        case SDLK_F7:
                        {
                            if (SDL_GetRelativeMouseMode() == SDL_bool(true))
                                SDL_SetRelativeMouseMode(SDL_bool(false));
                            else
                                SDL_SetRelativeMouseMode(SDL_bool(true));
                            break;
                        }
                        case SDLK_F8:
                        {
                            if(vertSyncOn)
                            {
                                vertSyncOn = false;
                                SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "0", SDL_HINT_OVERRIDE);
                                SDL_GL_SetSwapInterval(vertSyncOn);
                            }
                            else
                            {
                                vertSyncOn = true;
                                SDL_SetHintWithPriority(SDL_HINT_RENDER_VSYNC, "1", SDL_HINT_OVERRIDE);
                                SDL_GL_SetSwapInterval(vertSyncOn);
                            }                            
                            break;
                        }
                        case SDLK_F9:
                        {
                            fogOn = !(fogOn);
                            break;
                        }
                        case SDLK_F10:
                        {
                            ceilingOn = !(ceilingOn);
                            break;
                        }
                        case SDLK_F11:
                        {
                            debugColors = !(debugColors);
                            break;
                        }
                        case SDLK_F12:
                        {
                            if (SDL_GetWindowFlags(gwindow) & SDL_WINDOW_FULLSCREEN)
                            {
                                SDL_SetWindowFullscreen(gwindow, 0);
                                SDL_SetWindowSize(gwindow, gscreenWidth, gscreenHeight);
                                SDL_SetWindowPosition(gwindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            }
                            else
                            {
                                SDL_Rect rect;
                                SDL_GetDisplayBounds(0, &rect);
                                SDL_SetWindowSize(gwindow, rect.w, rect.h);
                                SDL_SetWindowFullscreen(gwindow, SDL_WINDOW_FULLSCREEN);
                            }
                            break;
                        }
                        default:
                            break;
                    }
                }
                break;
            }
            case(SDL_WINDOWEVENT):
            {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    resizeWindow(letterboxOn);
                }
                break;
            }
            default:
                break;
        }
    }
    
    moveSpeed = gDeltaTimer;
    if(moveSpeed == 0)
        moveSpeed = 1;
    moveSpeed = moveSpeed /(double)SDL_GetPerformanceFrequency() * 4; //effectively convert to seconds by /1000, then mult by 4. value is grid squares / sec
    double rotSpeed = moveSpeed/2; //the value is in radians/second
    if (mouseXDist > 0)
        rotSpeed *= mouseXDist * mouseSense;
    else if (mouseXDist < 0)
        rotSpeed *= mouseXDist * -mouseSense;
    double oldDirX = dirX;
    double oldPlaneX = planeX;
    if (mouseYDist > 0 && vertLook > ( (-1.0)*gscreenHeight / 2)) // look down
    {
        vertLook -= mouseYDist*mouseVertSense;
        if(vertLook < ( (-1.0)*gscreenHeight / 2))
            vertLook = ( (-1.0)*gscreenHeight / 2);
        calcFloorDist();
        int tw, th;
        SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th);
        gskySrcRect.y = (int)std::round(((double)th/2.0 - (double)gskySrcRect.h/2.0) - ((double)vertLook * ((double)gskySrcRect.h/(double)gskyDestRect.h)));
        if(gskySrcRect.y > th - gskySrcRect.h)
             gskySrcRect.y = th - gskySrcRect.h;

        gfloorRect.y = gscreenHeight / 2 + vertLook;
        gfloorRect.h = gscreenHeight - gfloorRect.y;

    }
    else if (mouseYDist < 0 && vertLook < (gscreenHeight / 2)) //look up
    {
        vertLook -= mouseYDist*mouseVertSense;
        if(vertLook > (gscreenHeight / 2))
            vertLook = (gscreenHeight / 2);
        calcFloorDist();
        int tw, th;
        SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th);
        gskySrcRect.y = (int)std::round(((double)th/2.0 - (double)gskySrcRect.h/2.0) - ((double)vertLook * ((double)gskySrcRect.h/(double)gskyDestRect.h)));
        if (gskySrcRect.y < 0)
            gskySrcRect.y = 0;

        gfloorRect.y = gscreenHeight / 2 + vertLook;
        gfloorRect.h = gscreenHeight - gfloorRect.y;
    }
    //when running forward or backward while strafing, your total displacement is effectively multplied by sqrt(2)
    //so we're just dividing speed by sqrt(2) in this situation to limit total displacement to normal values
    if (((currentKeyStates[SDL_SCANCODE_W] || currentKeyStates[SDL_SCANCODE_UP]) ^ (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN]))
       && ((currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT]) ^ (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT])))
    {
        moveSpeed *= 0.707;
    }
    //sprint by holding shift
    if (currentKeyStates[SDL_SCANCODE_LSHIFT] || currentKeyStates[SDL_SCANCODE_RSHIFT])
    {     
        moveSpeed *= 1.5;
    }    
    //Act on keypresses
    if (currentKeyStates[SDL_SCANCODE_ESCAPE]) //Pressed escape, close window
        quit = true;
    double xComponent = (dirX / (std::abs(dirX)+std::abs(dirY)));
    double yComponent = (dirY / (std::abs(dirX)+std::abs(dirY)));
    if (currentKeyStates[SDL_SCANCODE_W] || currentKeyStates[SDL_SCANCODE_UP]) //move forward
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX + xComponent * (0.3))][int(posY)].solid == false)
            if (leveldata[int(posX + xComponent * moveSpeed)][int(posY)].solid == false)
                posX += (xComponent) * moveSpeed;
        if (leveldata[int(posX)][int(posY + yComponent * (0.3))].solid == false)
            if (leveldata[int(posX)][int(posY + yComponent * moveSpeed)].solid == false)
                posY += yComponent * moveSpeed;
    }
    if (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN]) //move backward
    {
        if (leveldata[int(posX - xComponent * (0.3))][int(posY)].solid == false)
            if (leveldata[int(posX - xComponent * moveSpeed)][int(posY)].solid == false)
                posX -= xComponent * moveSpeed;
        if (leveldata[int(posX)][int(posY - yComponent * (0.3))].solid == false)
            if (leveldata[int(posX)][int(posY - yComponent * moveSpeed)].solid == false)
                posY -= yComponent * moveSpeed;
    }
    if (currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT]) //strafe left
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX - planeX * (0.3))][int(posY)].solid == false)
            if (leveldata[int(posX - planeX * moveSpeed)][int(posY)].solid == false)
                posX -= planeX * (moveSpeed);
        if (leveldata[int(posX)][int(posY - planeY * (0.3))].solid == false)
            if (leveldata[int(posX)][int(posY - planeY * moveSpeed)].solid == false)
                posY -= planeY * (moveSpeed);
    }
    if (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT]) //strafe right
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX + planeX * (0.3))][int(posY)].solid == false)
            if (leveldata[int(posX + planeX * moveSpeed)][int(posY)].solid == false)
                posX += planeX * moveSpeed;
        if (leveldata[int(posX)][int(posY + planeY * (0.3))].solid == false)
            if (leveldata[int(posX)][int(posY + planeY * moveSpeed)].solid == false)
                posY += planeY * moveSpeed;
    }
    if ((currentKeyStates[SDL_SCANCODE_Q] || mouseXDist < 0)&&currentKeyStates[SDL_SCANCODE_E]==false) //turn left
    {
        //redundant key checks prevent either key having priority
        //naively checking only one at a time can result in screen skew due to use of "oldDirX" and "oldPlaneX"
        //both camera direction and camera plane must be rotated
        dirX = dirX * cos(-rotSpeed) - dirY * sin(-rotSpeed);
        dirY = oldDirX * sin(-rotSpeed) + dirY * cos(-rotSpeed);
        planeX = planeX * cos(-rotSpeed) - planeY * sin(-rotSpeed);
        planeY = oldPlaneX * sin(-rotSpeed) + planeY * cos(-rotSpeed);
    }
    else if ((currentKeyStates[SDL_SCANCODE_E] || mouseXDist > 0)&& currentKeyStates[SDL_SCANCODE_Q]==false) //turn right
    {
        //both camera direction and camera plane must be rotated
        dirX = dirX * cos(rotSpeed) - dirY * sin(rotSpeed);
        dirY = oldDirX * sin(rotSpeed) + dirY * cos(rotSpeed);
        planeX = planeX * cos(rotSpeed) - planeY * sin(rotSpeed);
        planeY = oldPlaneX * sin(rotSpeed) + planeY * cos(rotSpeed);
    }
    if(currentKeyStates[SDL_SCANCODE_Z])
    {
        vertHeight -= (vertSpeed) * moveSpeed; //screen height corresponds to 1 absolute world unit
        if(vertHeight < (-0.2))
            vertHeight = (-0.2);
        calcFloorDist();
    }
    else if(currentKeyStates[SDL_SCANCODE_X])
    {
        vertHeight += (vertSpeed) * moveSpeed;
        if(vertHeight > (0.4))
            vertHeight = (0.4);
        calcFloorDist();
    }
    
    return quit;
}
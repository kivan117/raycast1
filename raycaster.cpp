#include <SDL2/SDL.h>
#include <stdio.h>
#include <cmath>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

#ifdef _WIN32
    const char PATH_SYM = '\\';
#else
    const char PATH_SYM = '/';
#endif

//some const values for our screen resolution
bool debugColors = false, ceilingOn = false, enableInput = false;
int mapWidth = 24;
int mapHeight = 24;
const int gscreenWidth =  960;//1920;//1280; //640; //720; //800; //960;
const int gscreenHeight = 540;//1080;//720; //360; //405; //450; //540;
int minBrightness = 0x01;
double torchBrightness = 1; // 0 - 1; multipler affecting torch light radius around player

bool vertSyncOn = true;
double floorDist[gscreenHeight];
double ceilDist[gscreenHeight];
double brightSin[gscreenWidth];

const double radToDeg = 180 / M_PI;
const double degToRad = M_PI / 180;

const double vFOV = 60.0;
double hFOV = 75.0;
int vertLook = 0;  // number of pixels to look up/down

bool sprinting = false;

double planeX = 0, planeY = std::tan((hFOV * degToRad)/2);

double posX = 2, posY = 2;         //x and y start position
double dirX = 1, dirY = 0;         //initial direction vector straight down

double moveSpeed = 0;
double viewTrip = 0.0;
double blockAheadDist = 500;
int blockAheadX = 0, blockAheadY = 0;
double mouseSense = 0.5;
double mouseVertSense = 2.0;
Uint64 oldtime = 0;
Uint64 gtime = 0;
unsigned int framecounter = 0;
SDL_Window *gwindow = NULL;
//The window renderer
SDL_Renderer *gRenderer = NULL;
SDL_Texture *gcurrTex = NULL; //the texture we're actually currently copying during render
SDL_Texture *gDoorTex = NULL; //test, wall, and sky textures hold texture data long term for examining later as needed
SDL_Texture **gwallTex = NULL;
SDL_Texture *gskyTex = NULL;
SDL_Texture *gfloorTex = NULL;
SDL_Texture *gceilTex = NULL;
SDL_Texture *gfloorBuffer = NULL;
SDL_Texture *gshadowTex = NULL;
SDL_Texture *weaponTex = NULL;
int gtexWidth = 0;
int gtexHeight = 0;
SDL_Rect gfloorRect;
SDL_Rect gskyDestRect;
SDL_Rect gskySrcRect;
SDL_Rect weaponTexRect = {0,0,0,0};
SDL_Rect weaponDestRect;
std::stringstream ssFPS;
std::vector<std::vector<int>> leveldata;

bool init();
bool initWindow();
bool initTextures();
void newlevel(bool warpView);
void loadLevel(std::string path);
bool update();
bool handleInput();
void updateScreen();

//calculate all raytracing steps and render the resulting walls
void calcRaycast();
//abstracted out the 2 types of possible world geometry drawing... the resulting functions are really ugly and need fixed because it as a quick hack
void drawWorldGeoFlat(double* wallDist, int* side, int* mapX, int* mapY);
void drawWorldGeoTex(double* wallDist, int* side, int* mapX, int* mapY);
void drawFloor(double* wallDist, int* side, int* mapX, int* mapY);
void close();
// loads a BMP image into a texture on the rendering device
SDL_Texture *loadTexture(const std::string &file, SDL_Renderer *ren);
// draw an SDL_texture to and SDL_renderer at position x,y
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, SDL_Rect dst, SDL_Rect *clip);
void renderTexture(SDL_Texture *tex, SDL_Renderer *ren, int x, int y, SDL_Rect *clip);
//get working directory, account for different folder symbol in windows paths
std::string getProjectPath(const std::string &subDir);
//load BMP, return texture
SDL_Texture *loadImage(std::string path);
SDL_Texture *loadImageColorKey(std::string path);
void generateShadowMask(SDL_Texture* tex, int texWidth, int texHeight);
void drawHud(); //just stub functions for now
void drawWeap();
void changeFOV(bool rel, double newFOV);

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
        printf("Something crucial broke. Rage quitting.");
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

    return success;
}

bool initWindow()
{
    bool success = true;
    //create window
    gwindow = SDL_CreateWindow("Raycast Test. FPS: ", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, gscreenWidth, gscreenHeight, SDL_WINDOW_RESIZABLE);
    if (gwindow == NULL) //window failed to create?
    {
        success = false;
        printf("SDL window creation unsuccessful. SDL_Error: %s\n", SDL_GetError());
    }
    else //successful window creation
    {
        //Create renderer for window
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        gRenderer = SDL_CreateRenderer(gwindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (gRenderer == NULL)
        {
            printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            success = false;
        }
        else
        {
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
#ifdef _WIN32
            //this nonsense is because of a bug with windows 10
            //without it, the mouse won't actually be captured
            //the relative input will work, but until the user minimizes and restores the window, it isn't captured
            SDL_MinimizeWindow(gwindow);
            SDL_RaiseWindow(gwindow);
            SDL_RestoreWindow(gwindow);
#endif
        }
    }
    return success;
}

bool initTextures()
{
    bool success = true;

    // loading in the textures
    gskyTex = loadImage("sky.bmp");
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
    std::stringstream texFileName;
    texFileName << "textures" << PATH_SYM << "floor0.bmp"; 
    gfloorTex = loadImage(texFileName.str());
    if (gfloorTex == NULL)
    {
        success = false;
    }
    texFileName.str(std::string());
    texFileName << "textures" << PATH_SYM << "ceil0.bmp";
    gceilTex = loadImage(texFileName.str());
    if (gceilTex == NULL)
    {
        success = false;
    }
    texFileName.str(std::string());
    texFileName << "textures" << PATH_SYM << "door0.bmp";
    gDoorTex = loadImage(texFileName.str());
    if (gDoorTex == NULL)
    {
        success = false;
    }
    else
    {
        gcurrTex = gDoorTex; // set the default texture
        SDL_QueryTexture(gcurrTex, NULL, NULL, &gtexWidth, &gtexHeight); //reset gcurrTex attributes
    }
    gwallTex = new SDL_Texture *[2];
    texFileName.str(std::string());
    texFileName << "textures" << PATH_SYM << "wall1.bmp";
    gwallTex[0] = loadImage(texFileName.str());
    texFileName.str(std::string());
    texFileName << "textures" << PATH_SYM << "wall2.bmp";
    gwallTex[1] = loadImage(texFileName.str());
    if (gwallTex[0] == NULL)
    {
        success = false;
    }
    if (gwallTex[1] == NULL)
    {
        success = false;
    }

    texFileName.str(std::string());
    texFileName << "sprites" << PATH_SYM << "shotgun1.bmp";
    weaponTex = loadImageColorKey(texFileName.str());
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
    gshadowTex = SDL_CreateTexture(gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, gscreenWidth, gscreenHeight);
    if(gshadowTex == NULL)
    {
        success = false;
    }
    else
    {
        for(int y = 0; y < gscreenHeight; y++) //define a height table for floor and ceiling calculations later
        {
            floorDist[y] = (gscreenHeight) / ((2.0*(y-vertLook) - (gscreenHeight)));
            ceilDist[y] = (gscreenHeight) / ((2.0*(y+vertLook) - (gscreenHeight)));
        }
        for(int x = 0; x < gscreenWidth; x++) //setup a sin lookup table for the shadow mask for later
        {
            brightSin[x] = sin((M_PI / 2.0)-(hFOV / 2.0 * degToRad)+(((double)x/(double)gscreenWidth)*(hFOV * degToRad)));
            brightSin[x] *= brightSin[x]; // squared to get stronger curve effect
        }
        generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
    }


    return success;
}

bool update()
{
    updateScreen();
    bool quit = handleInput();
    return quit;
}

bool handleInput()
{
    bool quit = false;
    bool moveFwd = false;
    int mouseXDist = 0, mouseYDist = 0;


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
            case(SDL_MOUSEMOTION):
            {
                if(enableInput)
                {
                    mouseXDist = e.motion.xrel;
                    mouseYDist = e.motion.yrel;
                }
                break;
            }
            case(SDL_MOUSEWHEEL):  //if scroll mousewheel, change horizontal field of view
            {
                if (enableInput)
                {
                    if (e.wheel.y > 0) // scroll up, increase FOV
                    {
                        changeFOV(true, 1.0);

                    }
                    else if (e.wheel.y < 0) // scroll down, decrease FOV
                    {
                        changeFOV(true, -1.0);
                    }
                    generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
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
                                //printf("Dist: %f, X: %d, Y: %d\n", blockAheadDist, blockAheadX, blockAheadY);
                                if (blockAheadDist < 1)
                                {
                                    switch (leveldata[blockAheadX][blockAheadY])
                                    {
                                    case 1:
                                        //printf("Solid brick, won't budge.\n");
                                        break;
                                    case 2:
                                        //printf("The exit.\n");
                                        newlevel(false);
                                        break;
                                    case 3:
                                        //printf("Opened Door\n");
                                        leveldata[blockAheadX][blockAheadY] = 0;
                                        break;
                                    default:
                                        //printf("Huh?\n");
                                        break;
                                    }
                                }
                            }
                            break;
                        }
                        case SDLK_DELETE:
                        {
                            if (mouseSense > 0.05)
                                mouseSense -= 0.05;
                            break;
                        }
                        case SDLK_INSERT:
                        {
                            mouseSense += 0.05;
                            if (mouseSense >= 5)
                                mouseSense = 5;
                            break;
                        }
                        case SDLK_HOME:
                        {
                            minBrightness = std::min(255, minBrightness+10);
                            generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
                            break;
                        }
                        case SDLK_END:
                        {
                            minBrightness = std::max(1, minBrightness-10);
                            generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
                            break;
                        }
                        case SDLK_PAGEUP:
                        {
                            torchBrightness = std::min(1.0, torchBrightness+0.04);
                            generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
                            break;
                        }
                        case SDLK_PAGEDOWN:
                        {
                            torchBrightness = std::max(0.04, torchBrightness-0.04);
                            generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
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
                            ceilingOn = !(ceilingOn);
                            break;
                        }
                        case SDLK_F10:
                        {
                            debugColors = !(debugColors);
                            break;
                        }
                        case SDLK_F11:
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
            /*case(SDL_KEYUP):
            {
                switch (e.key.keysym.sym)
                {
                    default:
                    {
                        break;
                    }
                }
                break;
            }*/
            case(SDL_WINDOWEVENT):
            {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED || e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    SDL_RenderSetLogicalSize(gRenderer, gscreenWidth, gscreenHeight);
                    int newWidth = 0;
                    int newHeight = 0;
                    SDL_GetWindowSize(gwindow, &newWidth, &newHeight);
                    SDL_RenderSetIntegerScale(gRenderer, SDL_bool(false));
                    float limitingSide = std::min((float)newWidth / gscreenWidth, (float)newHeight / gscreenHeight);
                    SDL_RenderSetScale(gRenderer, limitingSide, limitingSide);
                }
                break;
            }
            default:
                break;
        }
    }

    const Uint8 *currentKeyStates = SDL_GetKeyboardState(NULL);
    gtime = SDL_GetPerformanceCounter();
    moveSpeed = (gtime - oldtime);
    oldtime = gtime;
    if(++framecounter >= SDL_GetPerformanceFrequency() / moveSpeed / 5)
    {
        framecounter = 0;
        ssFPS << "Raycast Test. FPS: " << std::fixed << std::setprecision(2) << std::setfill('0') << std::setw(6) << (double)SDL_GetPerformanceFrequency() / moveSpeed
              << " | Vsync: " << vertSyncOn
              << " | FOV: "   << hFOV;
        SDL_SetWindowTitle(gwindow, ssFPS.str().c_str());
        ssFPS.str(std::string()); //blank the stream
    }
    if(moveSpeed == 0)
        moveSpeed = 1;
    
    moveSpeed = moveSpeed /(double)SDL_GetPerformanceFrequency() * 4; //effectively convert to seconds by /1000, then mult by 4. value is grid squares / sec
    double rotSpeed = moveSpeed/2; //the value is in radians/second
    if (mouseXDist > 0)
        rotSpeed *= mouseXDist * mouseSense;
    else if (mouseXDist < 0)
        rotSpeed *= mouseXDist * -mouseSense;
    if (mouseYDist > 0 && vertLook > ( (-1.0)*gscreenHeight / 2)) // look down
    {
        vertLook -= mouseYDist * mouseVertSense;
        if(vertLook < ( (-1.0)*gscreenHeight / 2))
            vertLook = ( (-1.0)*gscreenHeight / 2);
        for(int y = 0; y < gscreenHeight; y++) //define a height table for floor and ceiling calculations later
        {
            floorDist[y] = (gscreenHeight) / ((2.0*(y-vertLook) - (gscreenHeight)));
            ceilDist[y] = (gscreenHeight) / ((2.0*(y+vertLook) - (gscreenHeight)));
        }
        int tw, th;
        SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th);
        gskySrcRect.y = (int)std::round(((double)th/2.0 - (double)gskySrcRect.h/2.0) - ((double)vertLook * ((double)gskySrcRect.h/(double)gskyDestRect.h)));
        if(gskySrcRect.y > th - gskySrcRect.h)
             gskySrcRect.y = th - gskySrcRect.h;

        generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
    }
    else if (mouseYDist < 0 && vertLook < (gscreenHeight / 2)) //look up
    {
        vertLook -= mouseYDist * mouseVertSense;
        if(vertLook > (gscreenHeight / 2))
            vertLook = (gscreenHeight / 2);
        for(int y = 0; y < gscreenHeight; y++) //define a height table for floor and ceiling calculations later
        {
            floorDist[y] = (gscreenHeight) / ((2.0*(y-vertLook) - (gscreenHeight)));
            ceilDist[y] = (gscreenHeight) / ((2.0*(y+vertLook) - (gscreenHeight)));
        }
        int tw, th;
        SDL_QueryTexture(gskyTex, NULL, NULL, &tw, &th);
        gskySrcRect.y = (int)std::round(((double)th/2.0 - (double)gskySrcRect.h/2.0) - ((double)vertLook * ((double)gskySrcRect.h/(double)gskyDestRect.h)));
        if (gskySrcRect.y < 0)
            gskySrcRect.y = 0;
        
        generateShadowMask(gshadowTex, gscreenWidth, gscreenHeight);
    }
    double oldDirX = dirX;
    double oldPlaneX = planeX;

    //when running forward or backward while strafing, your total displacement is effectively multplied by sqrt(2)
    //so we're just dividing speed by sqrt(2) in this situation to limit total displacement to normal values
    if ((currentKeyStates[SDL_SCANCODE_W] ^ currentKeyStates[SDL_SCANCODE_S]) && (currentKeyStates[SDL_SCANCODE_A] ^ currentKeyStates[SDL_SCANCODE_D]))
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
    if (currentKeyStates[SDL_SCANCODE_W] || currentKeyStates[SDL_SCANCODE_UP]) //move forward
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX + dirX * (moveSpeed + 0.3))][int(posY)] == false)
            if (leveldata[int(posX + dirX * moveSpeed)][int(posY)] == false)
                posX += dirX * moveSpeed;
        if (leveldata[int(posX)][int(posY + dirY * (moveSpeed + 0.3))] == false)
            if (leveldata[int(posX)][int(posY + dirY * moveSpeed)] == false)
                posY += dirY * moveSpeed;
    }
    if (currentKeyStates[SDL_SCANCODE_S] || currentKeyStates[SDL_SCANCODE_DOWN]) //move backward
    {
        if (leveldata[int(posX - dirX * moveSpeed)][int(posY)] == false)
            if (leveldata[int(posX - dirX * moveSpeed)][int(posY)] == false)
                posX -= dirX * moveSpeed;
        if (leveldata[int(posX)][int(posY - dirY * (moveSpeed + 0.3))] == false)
            if (leveldata[int(posX)][int(posY - dirY * (moveSpeed + 0.3))] == false)
                posY -= dirY * moveSpeed;
    }
    if (currentKeyStates[SDL_SCANCODE_A] || currentKeyStates[SDL_SCANCODE_LEFT]) //strafe left
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX - planeX * (moveSpeed + 0.3))][int(posY)] == false)
            if (leveldata[int(posX - planeX * moveSpeed)][int(posY)] == false)
                posX -= planeX * (moveSpeed);
        if (leveldata[int(posX)][int(posY - planeY * (moveSpeed + 0.3 - (moveFwd * 0.1)))] == false)
            if (leveldata[int(posX)][int(posY - planeY * moveSpeed - (moveFwd * 0.1))] == false)
                posY -= planeY * (moveSpeed);
    }
    if (currentKeyStates[SDL_SCANCODE_D] || currentKeyStates[SDL_SCANCODE_RIGHT]) //strafe right
    {
        // the 0.3 is to try to prevent the player from normally being right up on the wall and clipping through it on corners
        // they still CAN, but they have to on purpose essentially
        if (leveldata[int(posX + planeX * (moveSpeed + 0.3))][int(posY)] == false)
            if (leveldata[int(posX + planeX * moveSpeed)][int(posY)] == false)
                posX += planeX * moveSpeed;
        if (leveldata[int(posX)][int(posY + planeY * (moveSpeed + 0.3))] == false)
            if (leveldata[int(posX)][int(posY + planeY * moveSpeed)] == false)
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

    return quit;
}
void updateScreen()
{
    //clear screen with a dark grey
    SDL_SetRenderDrawColor(gRenderer, 0x20, 0x20, 0x20, 0xff);
    SDL_RenderClear(gRenderer);

    calcRaycast(); //calculates and draws all raycast related screen updates

    drawHud();

    //Update screen
    SDL_RenderPresent(gRenderer);
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
            if (leveldata[mapX[x]][mapY[x]] > 0)
                hit = 1;
        }
        

        //Calculate distance to wall projected on camera direction (Euclidean distance will give fisheye effect!)
        if (side[x] == 0)
        {
            perpWallDist = (mapX[x] - posX + (1 - stepX) / 2) / rayDirX;
        }
        else
        {
            perpWallDist = (mapY[x] - posY + (1 - stepY) / 2) / rayDirY;
        }

        wallDist[x] = perpWallDist; //fill wall distance buffer
        if(x == gscreenWidth / 2)
        {
            blockAheadDist = perpWallDist;
            blockAheadX = mapX[x];
            blockAheadY = mapY[x];
        }

    }
    if(debugColors)
        drawWorldGeoFlat(wallDist, side, mapX, mapY);
    else
        drawWorldGeoTex(wallDist, side, mapX, mapY);

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
    for (int x = 0; x < gscreenWidth; x++)
        {
        lineHeight = (int)(gscreenHeight / wallDist[x]);
        //calculate lowest and highest pixel to fill in current stripe
        drawStart = -lineHeight / 2 + gscreenHeight / 2;
        drawEnd = lineHeight / 2 + gscreenHeight / 2;
        //choose wall color
        SDL_Color color;
        //give x and y sides different brightness
        if (side[x] == 0)
        {
            switch (leveldata[mapX[x]][mapY[x]])
            {
            case 1:
                color = {0xff, 0x00, 0x00, 0xff};
                break; //red
            case 2:
                color = {0x00, 0xff, 0x00, 0xff};
                break; //green
            case 3:
                color = {0x00, 0x00, 0xff, 0xff};
                break; //blue
            case 4:
                color = {0xff, 0xff, 0xff, 0xff};
                break; //white
            default:
                color = {0xff, 0xff, 0x00, 0xff};
                break; //yellow
            }
        }
        else
        {
            switch (leveldata[mapX[x]][mapY[x]])
            {
            case 1:
                color = {0x7f, 0x00, 0x00, 0xff};
                break; //red
            case 2:
                color = {0x00, 0x7f, 0x00, 0xff};
                break; //green
            case 3:
                color = {0x00, 0x00, 0x7f, 0xff};
                break; //blue
            case 4:
                color = {0x7f, 0x7f, 0x7f, 0xff};
                break; //white
            default:
                color = {0x7f, 0x7f, 0x00, 0xff};
                break; //yellow
            }
        }

        //calculate lowest and highest pixel to fill in current stripe
        drawStart = -lineHeight / 2 + gscreenHeight / 2;
        if (drawStart < 0)
            drawStart = 0;
        drawEnd = lineHeight / 2 + gscreenHeight / 2;
        if (drawEnd >= gscreenHeight)
            drawEnd = gscreenHeight - 1;

        //render vertical lines for raycast based on calculations

        SDL_SetRenderDrawColor(gRenderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(gRenderer, x, drawStart, x, drawEnd);
    }
}

void drawWorldGeoTex(double* wallDist, int* side, int* mapX, int* mapY)  /* TODO: fix hardcoded magic numbers for skybox to be FOV aware */
{
    if(ceilingOn == false)
    {
        //Here I'm creating a sky box and rotating it according to player's viewing angle
        //trying to match drawn sky segment to FOV
        //skybox is 900 pix wide, FOV is ~75 degrees, so we can see ~188 pixels at a time

        //all of these hard coded values are dependent on the sky texture resolution, but not the game resolution
        //altering the game FOV will make the values wrong though
        //need to make it calculated based on FOV and sky texture dimensions
        
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

    drawFloor(wallDist, side, mapX, mapY);
    double cameraX, rayDirX, rayDirY, wallX;
    int lineHeight, brightness, texX, drawStart, drawEnd;
    
    for (int x = 0; x < gscreenWidth; x++)
    {
        cameraX = 2 * x / double(gscreenWidth) - 1; //x-coordinate in camera space, or along the x of the camera plane itself
        rayDirX = dirX + planeX * cameraX; //the XY coord where the vector of this ray crosses the camera plane
        rayDirY = dirY + planeY * cameraX;
        //Calculate height of line to draw on screen
        lineHeight = (int)(gscreenHeight / wallDist[x]);

        if(ceilingOn) //if we're drawing the ceiling, use ambiant light and torch brightness to set wall brightness
        {
            brightness = (int)std::max(std::min(255.0, lineHeight * torchBrightness * brightSin[x]), (double)minBrightness);
        }
        else //if no ceiling, use north-south vs east-west wall to set wall brightness
        {
            if(side[x] == 0)
                brightness = 0xff;
            else
                brightness = 0x80;
        }
        //choose a texture
        switch (leveldata[mapX[x]][mapY[x]])
        {
        case 1:
            gcurrTex = gwallTex[0];
            SDL_QueryTexture(gcurrTex, NULL, NULL, &gtexWidth, &gtexHeight);
            break; //"red" flat color
        case 2:
            gcurrTex = gwallTex[1];
            SDL_QueryTexture(gcurrTex, NULL, NULL, &gtexWidth, &gtexHeight);
            break; //"green"
        default:
            gcurrTex = gDoorTex;
            SDL_QueryTexture(gcurrTex, NULL, NULL, &gtexWidth, &gtexHeight);
            break; //"blue"
        }

        //calculate value of wallX
        if (side[x] == 0)
            wallX = posY + wallDist[x] * rayDirY; //if we hit a NS wall, use y pos, + perpendicular value * y component of vector to get total y offset
        else
            wallX = posX + wallDist[x] * rayDirX; //as above, but x value for EW walls
        wallX -= floor((wallX));                   //subtract away the digits to the left of the decimal point, leaving only the fractional value across the single wall

        //x coordinate on the texture
        texX = int(wallX * double(gtexWidth)); //determine exact value across the wall texture in pixels
        if (side[x] == 0 && rayDirX < 0)
            texX = gtexWidth - texX - 1; //horizontally flip textures so they're drawn properly depending on the side of the cube they're on
        if (side[x] == 1 && rayDirY > 0)
            texX = gtexWidth - texX - 1;

        //calculate lowest and highest pixel to fill in current stripe
        drawStart = -lineHeight / 2 + (gscreenHeight / 2) + vertLook;
        drawEnd = lineHeight / 2 + (gscreenHeight / 2) + vertLook;

        // set up the rectangle to sample the texture for the wall
        SDL_Rect line = {x, drawStart, 1, drawEnd - drawStart};
        SDL_Rect sample = {texX, 0, 1, gtexHeight};

        //use color mod to darken the wall texture
        //255 = no color mod, lower values mean darker
        //currently based on distance from player, but you can do x/y side of walls or whatever
        SDL_SetTextureColorMod(gcurrTex, brightness, brightness, brightness);
        //draw wall stripe
        renderTexture(gcurrTex, gRenderer, line, &sample);

    }
}

void drawFloor(double* wallDist, int* side, int* mapX, int* mapY)
{
    //create some pointers to later access the pixels in the floor and buffer textures
    void *floorBufferPixels, *floorTexPixels, *ceilTexPixels;
    int floorBufferPitch, floorTexPitch, ceilTexPitch, floorTexX, floorTexY, lineHeight, drawEnd;
    //Uint32 currentColor;

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
        //Calculate height of line to draw on screen
        lineHeight = (int)(gscreenHeight / wallDist[x]);
        //calculate lowest and highest pixel to fill in current stripe
        drawEnd = (lineHeight / 2) + (gscreenHeight / 2) + vertLook;
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

        if (drawEnd > gscreenHeight || drawEnd < 0)
            drawEnd = gscreenHeight; //becomes < 0 when the integer overflows

        //draw the floor from drawEnd to the bottom of the screen
        for (int y = 0; y < drawEnd; y++)
        {
            bufferPixels[y * gscreenWidth + x] = 0;
        }
        SDL_QueryTexture(gfloorTex, NULL, NULL, &gtexWidth, &gtexHeight);
        for (int y = drawEnd; y < gscreenHeight; y++)
        {

            weight = (floorDist[y] - distPlayer) / (wallDist[x] - distPlayer);
            currentFloorX = weight * floorXWall + (1.0 - weight) * posX;
            currentFloorY = weight * floorYWall + (1.0 - weight) * posY;
            floorTexX = int(currentFloorX * gtexWidth) % gtexWidth;
            floorTexY = int(currentFloorY * gtexHeight) % gtexHeight;

            //shitty slow version. need to find a way to go faster than streaming textures and a buffer texture
            bufferPixels[y * gscreenWidth + x] = ufloorTexPix[gtexWidth * floorTexY + floorTexX];
        }
        
        if(ceilingOn)
        {
            drawEnd = (lineHeight / 2) + (gscreenHeight / 2) + vertLook;
            SDL_QueryTexture(gceilTex, NULL, NULL, &gtexWidth, &gtexHeight);
            for (int y = gscreenHeight - drawEnd + lineHeight; y < gscreenHeight; y++)
            {
                
                weight = (ceilDist[y] - distPlayer) / (wallDist[x] - distPlayer);
                currentFloorX = weight * floorXWall + (1.0 - weight) * posX;
                currentFloorY = weight * floorYWall + (1.0 - weight) * posY;
                floorTexX = int(currentFloorX * gtexWidth) % gtexWidth;
                floorTexY = int(currentFloorY * gtexHeight) % gtexHeight;

                bufferPixels[(gscreenHeight - y - 1) * gscreenWidth + x] = uceilTexPix[gtexWidth * floorTexY + floorTexX];
            }
        }
    }

    //Render floor by unlocking floor textures and buffer, and copying the entire buffer onto the render target in one go
    //Unlock texture
    SDL_UnlockTexture(gfloorTex);
    SDL_UnlockTexture(gceilTex);
    SDL_UnlockTexture(gfloorBuffer);
     //SDL_FreeFormat(mappingFormat);
    SDL_SetTextureBlendMode(gfloorBuffer, SDL_BLENDMODE_BLEND);
    SDL_RenderCopy(gRenderer, gfloorBuffer, NULL, NULL);    
    //render distance shadow on top of floor/ceiling textures
    if(ceilingOn)
    {
        SDL_SetTextureBlendMode(gshadowTex, SDL_BLENDMODE_MOD);
        SDL_RenderCopy(gRenderer, gshadowTex, NULL, NULL);
    }


}

void close()
{
    //destroy renderer
    SDL_DestroyTexture(gskyTex);
    SDL_DestroyTexture(gDoorTex);
    SDL_DestroyTexture(gwallTex[0]);
    SDL_DestroyTexture(gwallTex[1]);
    SDL_DestroyTexture(gfloorTex);
    SDL_DestroyTexture(gceilTex);
    SDL_DestroyTexture(weaponTex);
    gskyTex = NULL;
    gDoorTex = NULL;
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
            printf("Error getting resource path: %s\n", SDL_GetError());
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

SDL_Texture *loadImageColorKey(std::string path)
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
        SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0xff, 0x00, 0xff));
        tex = SDL_CreateTextureFromSurface(gRenderer,bmp);// (gRenderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, bmp->w, bmp->h);

        SDL_FreeSurface(bmp);
        if (tex == nullptr)
        {
            printf("SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        }
    }

    return tex;
}

void generateShadowMask(SDL_Texture* tex, int texWidth, int texHeight)
{

    void* texPix = NULL;
    int pitch;
    SDL_LockTexture(tex, NULL, &texPix, &pitch);
    Uint32* pixels = (Uint32*)texPix;
    double brightness;
    Uint32 format;
    SDL_QueryTexture(tex, &format, NULL, NULL, NULL);
    SDL_PixelFormat *mappingFormat = SDL_AllocFormat(format);
    Uint8 tempBright = 0; //variable to offset torchbrightness by sin lookup table for each column, giving a nice curved radius to the torch fade
    for(int y = 0; y < texHeight; y++)
    {
        if(y < (texHeight / 2) + vertLook)
        {
            brightness = std::min(255.0,255.0*std::max((double)minBrightness/255.0,torchBrightness/(ceilDist[texHeight-y])));
            for(int x = 0; x < texWidth; x++)
            {
                tempBright = (Uint8)std::min(255.0,std::max((double)minBrightness, brightness * brightSin[x]));
                pixels[y * texWidth + x] = SDL_MapRGBA(mappingFormat, tempBright, tempBright, tempBright, 0xff);
                //pixels[(texHeight - y - 1) * texWidth + x] = SDL_MapRGBA(mappingFormat, tempBright, tempBright, tempBright, 0xff);
            }
        }
        else
        {
            brightness = std::min(255.0,255.0*std::max((double)minBrightness/255.0,torchBrightness/(floorDist[y])));
            for(int x = 0; x < texWidth; x++)
            {
                tempBright = (Uint8)std::min(255.0,std::max((double)minBrightness, brightness * brightSin[x]));
                //pixels[y * texWidth + x] = SDL_MapRGBA(mappingFormat, tempBright, tempBright, tempBright, 0xff);
                pixels[(y) * texWidth + x] = SDL_MapRGBA(mappingFormat, tempBright, tempBright, tempBright, 0xff);
            }
        }
        
    }
    SDL_FreeFormat(mappingFormat);
    SDL_UnlockTexture(tex);

    return;
}

void drawHud()
{
    drawWeap();
    return;
}
void drawWeap()
{
    renderTexture(weaponTex, gRenderer, weaponDestRect, &weaponTexRect);
    return;
}

void newlevel(bool warpView)
{
        enableInput = false; //shouldn't matter but just in case, disable player input during new level load
        if(warpView)
        {
            for(int i = 0; i < 50; i++)
            {
                viewTrip -= 0.2; //make floor and ceiling appear to stretch / fall away. acid trip effect

                updateScreen();
                SDL_Delay(20);
            }
        }

        //randomly load a new level from maps directory, named map0.txt to map19.txt
        //eventually this might get replced with the random level gen function or something else
        std::stringstream levelFileName;
        srand(time(0));
        levelFileName << getProjectPath("maps") << "map" << std::to_string(rand() % 20) << ".txt";
        ceilingOn = rand()%2;
        printf("Map: %s\n", levelFileName.str().c_str());
        loadLevel(levelFileName.str());

        //reset all the camera stuff
        dirX = 1;
        dirY = 0;        
        planeX = 0;
        planeY = std::tan((hFOV*degToRad)/2);
        viewTrip = 0;
        enableInput = true;
}

void loadLevel(std::string path)
{
    // static std::string projectPath = getProjectPath();

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
            leveldata.at(x).at(y) = a; //hope this works?
        }
    }

    mapFile.close();
}

void changeFOV(bool rel, double newFOV)
{
    if(rel)
    {
        double mult = (hFOV + newFOV)/hFOV;
        hFOV += newFOV;
        planeY *= mult;
        planeX *= mult;
    }
    else
    {
        double mult = newFOV/hFOV;
        hFOV = newFOV;
        planeY *= mult;
        planeX *= mult;
    }
    
}
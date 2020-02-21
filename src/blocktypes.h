#include <vector>

const int totalBlocks = 64;

struct Map_Block {
    int block_id = 0; // index / name of block
    bool solid = true; //wall has collision or not
    bool visible = false; //are walls visible (doesn't affect floor and ceil yet)
    bool transparent = false;
    int wallTex[4] = {0, 0, 0 , 0}; //NSEW wall texture index numbers
    int floorTex = 0; //texture for floor on this block
    int ceilTex = 0; //texture for ceiling on this block
    double height = 1.0;
    int alphaMod = 255; //out of 255
};

enum WALL_DIR {NORTH, SOUTH, EAST, WEST};
enum BLOCK_IDS {    BLOCK_AIR,
                    BLOCK_WALL,
                    BLOCK_PANEL,
                    BLOCK_DOOR,
                    BLOCK_WINDOW };

std::vector<Map_Block> blockTypes;

void initBlockTypes()
{
    if(blockTypes.size() < totalBlocks)
    {
        blockTypes.resize(totalBlocks);
    }
    // BLOCK 0 - EMTPY SPACE
    blockTypes.at(BLOCK_AIR).block_id = BLOCK_AIR;
    blockTypes.at(BLOCK_AIR).visible = false;
    blockTypes.at(BLOCK_AIR).solid = false;
    blockTypes.at(BLOCK_AIR).transparent = false;
    blockTypes.at(BLOCK_AIR).wallTex[NORTH] = 0;
    blockTypes.at(BLOCK_AIR).wallTex[SOUTH] = 0;
    blockTypes.at(BLOCK_AIR).wallTex[EAST] = 0;
    blockTypes.at(BLOCK_AIR).wallTex[WEST] = 0;
    blockTypes.at(BLOCK_AIR).floorTex = 0;
    blockTypes.at(BLOCK_AIR).ceilTex = 0;
    blockTypes.at(BLOCK_AIR).height = 1.0;
    blockTypes.at(BLOCK_AIR).alphaMod = 255;

    // BLOCK 1 - SOLID WALL TYPE 1
    blockTypes.at(BLOCK_WALL).block_id = BLOCK_WALL;
    blockTypes.at(BLOCK_WALL).visible = true;
    blockTypes.at(BLOCK_WALL).solid = true;
    blockTypes.at(BLOCK_WALL).transparent = false;
    blockTypes.at(BLOCK_WALL).wallTex[NORTH] = 0;
    blockTypes.at(BLOCK_WALL).wallTex[SOUTH] = 0;
    blockTypes.at(BLOCK_WALL).wallTex[EAST] = 0;
    blockTypes.at(BLOCK_WALL).wallTex[WEST] = 0;
    blockTypes.at(BLOCK_WALL).floorTex = 0;
    blockTypes.at(BLOCK_WALL).ceilTex = 0;
    blockTypes.at(BLOCK_WALL).height = 1.0;
    blockTypes.at(BLOCK_WALL).alphaMod = 255;

    // BLOCK 2 - EXIT PANEL
    blockTypes.at(BLOCK_PANEL).block_id = BLOCK_PANEL;
    blockTypes.at(BLOCK_PANEL).visible = true;
    blockTypes.at(BLOCK_PANEL).solid = true;
    blockTypes.at(BLOCK_PANEL).transparent = false;
    blockTypes.at(BLOCK_PANEL).wallTex[NORTH] = 1;
    blockTypes.at(BLOCK_PANEL).wallTex[SOUTH] = 1;
    blockTypes.at(BLOCK_PANEL).wallTex[EAST] = 1;
    blockTypes.at(BLOCK_PANEL).wallTex[WEST] = 1;
    blockTypes.at(BLOCK_PANEL).floorTex = 0;
    blockTypes.at(BLOCK_PANEL).ceilTex = 0;
    blockTypes.at(BLOCK_PANEL).height = 1.0;
    blockTypes.at(BLOCK_PANEL).alphaMod = 255;

    // BLOCK 3 - DOOR TYPE 1
    blockTypes.at(BLOCK_DOOR).block_id = BLOCK_DOOR;
    blockTypes.at(BLOCK_DOOR).visible = true;
    blockTypes.at(BLOCK_DOOR).solid = true;
    blockTypes.at(BLOCK_DOOR).transparent = true;
    blockTypes.at(BLOCK_DOOR).wallTex[NORTH] = 2;
    blockTypes.at(BLOCK_DOOR).wallTex[SOUTH] = 2;
    blockTypes.at(BLOCK_DOOR).wallTex[EAST] = 2;
    blockTypes.at(BLOCK_DOOR).wallTex[WEST] = 2;
    blockTypes.at(BLOCK_DOOR).floorTex = 0;
    blockTypes.at(BLOCK_DOOR).ceilTex = 0;
    blockTypes.at(BLOCK_DOOR).height = 1.0;
    blockTypes.at(BLOCK_DOOR).alphaMod = 255;

    // BLOCK 3 - WINDOW
    blockTypes.at(BLOCK_WINDOW).block_id = BLOCK_WINDOW;
    blockTypes.at(BLOCK_WINDOW).visible = true;
    blockTypes.at(BLOCK_WINDOW).solid = true;
    blockTypes.at(BLOCK_WINDOW).transparent = true;
    blockTypes.at(BLOCK_WINDOW).wallTex[NORTH] = 3;
    blockTypes.at(BLOCK_WINDOW).wallTex[SOUTH] = 3;
    blockTypes.at(BLOCK_WINDOW).wallTex[EAST] = 3;
    blockTypes.at(BLOCK_WINDOW).wallTex[WEST] = 3;
    blockTypes.at(BLOCK_WINDOW).floorTex = 0;
    blockTypes.at(BLOCK_WINDOW).ceilTex = 0;
    blockTypes.at(BLOCK_WINDOW).height = 1.0;
    blockTypes.at(BLOCK_WINDOW).alphaMod = 255;
}

//change a map block's id and internal settings
void changeBlock(Map_Block* blockIn, unsigned int newID)
{
    if (newID < blockTypes.size())
    {
        blockIn->block_id = newID;
        blockIn->solid = blockTypes[newID].solid;
        blockIn->visible = blockTypes[newID].visible;
        blockIn->transparent = blockTypes[newID].transparent;
        blockIn->wallTex[NORTH] = blockTypes[newID].wallTex[NORTH];
        blockIn->wallTex[SOUTH] = blockTypes[newID].wallTex[SOUTH];
        blockIn->wallTex[EAST] = blockTypes[newID].wallTex[EAST];
        blockIn->wallTex[WEST] = blockTypes[newID].wallTex[WEST];
        blockIn->floorTex = blockTypes[newID].floorTex;
        blockIn->ceilTex = blockTypes[newID].ceilTex;
        blockIn->height = blockTypes[newID].height;
        blockIn->alphaMod = blockTypes[newID].alphaMod;
    }
    return;
}
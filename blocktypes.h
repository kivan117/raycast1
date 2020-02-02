#include <vector>

const int totalBlocks = 64;

struct Map_Block {
    int block_id = 0;
    bool solid = 0;
    bool visible = 0;
    int wallTex[4] = {0, 0, 0 , 0};
    int floorTex = 0;
    int ceilTex = 0;
};

std::vector<Map_Block> blockTypes;

void initBlockTypes()
{
    if(blockTypes.size() < totalBlocks)
    {
        blockTypes.resize(totalBlocks);
    }
    // BLOCK 0 - EMTPY SPACE
    blockTypes.at(0).block_id = 0;
    blockTypes.at(0).visible = 0;
    blockTypes.at(0).solid = 0;
    blockTypes.at(0).wallTex[0] = 0;
    blockTypes.at(0).wallTex[1] = 0;
    blockTypes.at(0).wallTex[2] = 0;
    blockTypes.at(0).wallTex[3] = 0;
    blockTypes.at(0).floorTex = 0;
    blockTypes.at(0).ceilTex = 0;

    // BLOCK 1 - SOLID WALL TYPE 1
    blockTypes.at(1).block_id = 1;
    blockTypes.at(1).visible = 1;
    blockTypes.at(1).solid = 1;
    blockTypes.at(1).wallTex[0] = 0;
    blockTypes.at(1).wallTex[1] = 0;
    blockTypes.at(1).wallTex[2] = 0;
    blockTypes.at(1).wallTex[3] = 0;
    blockTypes.at(1).floorTex = 0;
    blockTypes.at(1).ceilTex = 0;

    // BLOCK 2 - EXIT PANEL
    blockTypes.at(2).block_id = 2;
    blockTypes.at(2).visible = 1;
    blockTypes.at(2).solid = 1;
    blockTypes.at(2).wallTex[0] = 1;
    blockTypes.at(2).wallTex[1] = 1;
    blockTypes.at(2).wallTex[2] = 1;
    blockTypes.at(2).wallTex[3] = 1;
    blockTypes.at(2).floorTex = 0;
    blockTypes.at(2).ceilTex = 0;

    // BLOCK 3 - DOOR TYPE 1
    blockTypes.at(3).block_id = 3;
    blockTypes.at(3).visible = 1;
    blockTypes.at(3).solid = 1;
    blockTypes.at(3).wallTex[0] = 2;
    blockTypes.at(3).wallTex[1] = 2;
    blockTypes.at(3).wallTex[2] = 2;
    blockTypes.at(3).wallTex[3] = 2;
    blockTypes.at(3).floorTex = 0;
    blockTypes.at(3).ceilTex = 0;
}

//change a map block's id and internal settings
void changeBlock(Map_Block* blockIn, int newID)
{
    if (newID < blockTypes.size())
    {
        blockIn->block_id = newID;
        blockIn->solid = blockTypes[newID].solid;
        blockIn->visible = blockTypes[newID].visible;
        blockIn->wallTex[0] = blockTypes[newID].wallTex[0];
        blockIn->wallTex[1] = blockTypes[newID].wallTex[1];
        blockIn->wallTex[2] = blockTypes[newID].wallTex[2];
        blockIn->wallTex[3] = blockTypes[newID].wallTex[3];
        blockIn->floorTex = blockTypes[newID].floorTex;
        blockIn->ceilTex = blockTypes[newID].ceilTex;
    }
    return;
}
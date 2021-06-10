#include "controller_keyboard_debug.h"
#include "game/level_update.h"
#include "game/camera.h"
#include "pc/network/network.h"
#include "game/mario.h"
#include "sm64.h"

#ifdef DEBUG

static u8 warpToLevel = LEVEL_CCM;

#define SCANCODE_0 0x0B
#define SCANCODE_1 0x02
#define SCANCODE_2 0x03
#define SCANCODE_3 0x04
#define SCANCODE_4 0x05
#define SCANCODE_5 0x06
#define SCANCODE_6 0x07
#define SCANCODE_7 0x08
#define SCANCODE_8 0x09
#define SCANCODE_9 0x0A

static void debug_breakpoint_here(void) {
    // create easy breakpoint position for debugging
}

static void debug_warp_level(u8 level) {
    if (sCurrPlayMode == PLAY_MODE_CHANGE_LEVEL) { return; }

    // find level from painting
    for (int i = 0; i < 45; i++) {
        struct WarpNode* node = &gCurrentArea->paintingWarpNodes[i];
        if (node == NULL) { break; }
        if (node->destLevel == level) {
            sWarpDest.type = WARP_TYPE_CHANGE_LEVEL;
            sWarpDest.levelNum = node->destLevel;
            sWarpDest.areaIdx = node->destArea;
            sWarpDest.nodeId = node->destNode;
            sWarpDest.arg = 0;

            sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
            return;
        }
    }

    struct ObjectWarpNode* objectNode = gCurrentArea->warpNodes;
    while (objectNode != NULL) {
        struct WarpNode* node = &objectNode->node;
        if (node->destLevel == level) {
            sWarpDest.type = WARP_TYPE_CHANGE_LEVEL;
            sWarpDest.levelNum = node->destLevel;
            sWarpDest.areaIdx = node->destArea;
            sWarpDest.nodeId = node->destNode;
            sWarpDest.arg = 0;

            sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
            return;
        }
        objectNode = objectNode->next;
    }

    // failed, go to main castle area
    sWarpDest.type = WARP_TYPE_CHANGE_LEVEL;
    sWarpDest.levelNum = LEVEL_CASTLE;
    sWarpDest.areaIdx = 1;
    sWarpDest.nodeId = 0x1F;
    sWarpDest.arg = 0;
    sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
    D_80339ECA = 0;
    D_80339EE0 = 0;
    extern s16 gSavedCourseNum;
    gSavedCourseNum = 0;
}

static void debug_warp_area() {
    if (sCurrPlayMode == PLAY_MODE_CHANGE_LEVEL) { return; }

    struct ObjectWarpNode* objectNode = gCurrentArea->warpNodes;
    while (objectNode != NULL) {
        struct WarpNode* node = &objectNode->node;
        if (node->destLevel == gCurrLevelNum && node->destArea != gCurrAreaIndex) {
            sWarpDest.type = WARP_TYPE_CHANGE_AREA;
            sWarpDest.levelNum = node->destLevel;
            sWarpDest.areaIdx = node->destArea;
            sWarpDest.nodeId = node->destNode;
            sWarpDest.arg = 0;

            sCurrPlayMode = PLAY_MODE_CHANGE_LEVEL;
            return;
        }
        objectNode = objectNode->next;
    }
}

static void debug_grand_star(void) {
    set_mario_action(&gMarioStates[0], ACT_JUMBO_STAR_CUTSCENE, 0);
}

static void debug_warp_to(void) {
    gMarioStates[0].pos[0] = gMarioStates[1].pos[0];
    gMarioStates[0].pos[1] = gMarioStates[1].pos[1];
    gMarioStates[0].pos[2] = gMarioStates[1].pos[2];
    gMarioStates[0].marioObj->oRoom = gMarioStates[1].marioObj->oRoom;
}

static void debug_suicide(void) {
    gMarioStates[0].hurtCounter = 31;
}

void debug_keyboard_on_key_down(int scancode) {
    scancode = scancode;
    switch (scancode & 0xFF) {
        case SCANCODE_3: debug_breakpoint_here(); break;
#ifdef DEVELOPMENT
        case SCANCODE_6: debug_warp_level(warpToLevel); break;
        case SCANCODE_7: debug_warp_area(); break;
        case SCANCODE_9: debug_warp_to(); break;
        case SCANCODE_0: debug_suicide(); break;
#endif
    }
}

#endif
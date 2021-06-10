#include <stdio.h>
#include "../network.h"
#include "game/interaction.h"
#include "game/object_list_processor.h"
#include "game/object_helpers.h"
#include "game/interaction.h"
#include "game/level_update.h"
#include "game/macro_special_objects.h"
#include "macro_presets.h"
#include "object_constants.h"
#include "object_fields.h"
#include "behavior_table.h"
#include "model_ids.h"
#define DISABLE_MODULE_LOG 1
#include "pc/debuglog.h"

static struct Object* get_object_matching_respawn_info(s16* respawnInfo) {
    for (int i = 0; i < OBJECT_POOL_CAPACITY; i++) {
        struct Object* o = &gObjectPool[i];
        if (o->respawnInfo == respawnInfo) { return o; }
    }

    return NULL;
}

////

void network_send_macro_deletions_area(u8 destGlobalIndex, u8 areaIndex) {
    // check that the area is active
    struct Area* area = &gAreaData[areaIndex];
    if (area->unk04 == NULL) { return; }

    struct NetworkPlayer* destNp = network_player_from_global_index(destGlobalIndex);
    if (destNp == NULL || !destNp->connected) {
        LOG_ERROR("network_send_macro_deletions_area: dest np is invalid");
        return;
    }

    // write header
    struct Packet p;
    packet_init(&p, PACKET_MACRO_DELETIONS, true, false);
    packet_write(&p, &gCurrCourseNum, sizeof(s16));
    packet_write(&p, &gCurrActNum, sizeof(s16));
    packet_write(&p, &gCurrLevelNum, sizeof(s16));
    packet_write(&p, &gCurrAreaIndex, sizeof(s16));

    // write this area's index
    packet_write(&p, &areaIndex, sizeof(u8));

    // write the amount of deletions
    u8 zero = 0;
    u8* macroDeletionCount = &p.buffer[p.cursor];
    packet_write(&p, &zero, sizeof(u8));

    // loop through macro objects for deletions
    s16* macroObjList = area->macroObjects;
    while (*macroObjList != -1) {
        // grab preset ID
        s32 presetID = (*macroObjList & 0x1FF) - 31; // Preset identifier for MacroObjectPresets array
        if (presetID < 0) { break; }

        // parse respawn info
        macroObjList += 4;
        s16* respawnInfo = macroObjList++;

        // if a macro object was destroyed, send its respawnInfo offset
        if (((*respawnInfo >> 8) & RESPAWN_INFO_DONT_RESPAWN) == RESPAWN_INFO_DONT_RESPAWN) {
            *macroDeletionCount = *macroDeletionCount + 1;
            u16 offset = respawnInfo - area->macroObjects;
            packet_write(&p, &offset, sizeof(u16));
            LOG_INFO("tx macro deletion: offset %d", offset);
        }
    }

    // write the amount of special cases
    u8* macroSpecialCount = &p.buffer[p.cursor];
    packet_write(&p, &zero, sizeof(u8));

    // loop through macro objects for special cases
    macroObjList = area->macroObjects;
    while (*macroObjList != -1) {
        // grab preset ID
        s32 presetID = (*macroObjList & 0x1FF) - 31; // Preset identifier for MacroObjectPresets array
        if (presetID < 0) { break; }

        // parse respawn info
        macroObjList += 4;
        s16* respawnInfo = macroObjList++;

        // check for special cases
        const BehaviorScript* behavior = MacroObjectPresets[presetID].behavior;
        if (behavior == bhvCoinFormation && *respawnInfo != 0) {
            *macroSpecialCount = *macroSpecialCount + 1;
            u16 offset = respawnInfo - area->macroObjects;
            packet_write(&p, &offset,     sizeof(u16));
            packet_write(&p, respawnInfo, sizeof(s16));
            LOG_INFO("tx macro special: offset %d, respawnInfo %d", offset, *respawnInfo);
        }
    }

    // send the packet if there are deletions
    if (*macroDeletionCount > 0 || *macroSpecialCount > 0) {
        network_send_to(destNp->localIndex, &p);
        LOG_INFO("tx macro deletion for area %d (count %d)", areaIndex, *macroDeletionCount);
    }
}

void network_send_macro_deletions(u8 destGlobalIndex) {
    if (!gNetworkPlayerLocal->currAreaSyncValid) {
        LOG_ERROR("my area is invalid");
        return;
    }

    struct NetworkPlayer* destNp = network_player_from_global_index(destGlobalIndex);
    if (destNp == NULL || !destNp->connected) {
        LOG_ERROR("network_send_macro_deletions: dest np is invalid");
        return;
    }

    for (int i = 0; i < 8; i++) {
        network_send_macro_deletions_area(destGlobalIndex, i);
    }
}

void network_receive_macro_deletions(struct Packet* p) {
    s16 courseNum, actNum, levelNum, areaIndex;
    packet_read(p, &courseNum,       sizeof(s16));
    packet_read(p, &actNum,          sizeof(s16));
    packet_read(p, &levelNum,        sizeof(s16));
    packet_read(p, &areaIndex,       sizeof(s16));

    if (courseNum != gCurrCourseNum || actNum != gCurrActNum || levelNum != gCurrLevelNum) {
        LOG_ERROR("Receiving 'location response' with the wrong location!");
        return;
    }

    u8 thisAreaIndex;
    packet_read(p, &thisAreaIndex, sizeof(u8));

    // read and execute macro deletions
    u8 macroDeletionCount;
    packet_read(p, &macroDeletionCount, sizeof(u8));
    LOG_INFO("rx macro deletions (count %d)", macroDeletionCount);

    while (macroDeletionCount-- > 0) {
        u16 offset;
        packet_read(p, &offset, sizeof(u16));
        LOG_INFO("rx macro deletion: offset %d", offset);

        // mark respawninfo as dont respawn
        s16* respawnInfo = gAreaData[thisAreaIndex].macroObjects + offset;
        *respawnInfo |= RESPAWN_INFO_DONT_RESPAWN << 8;

        struct Object* o = get_object_matching_respawn_info(respawnInfo);
        if (o != NULL) {
            obj_mark_for_deletion(o);
            LOG_INFO("rx macro deletion: object");
            if (o->oSyncID != 0) {
                struct SyncObject* so = &gSyncObjects[o->oSyncID];
                if (so->o == o) {
                    network_forget_sync_object(so);
                    LOG_INFO("rx macro deletion: sync object");
                }
            }
        }
    }

    // read and execute macro specials

    u8 macroSpecialCount;
    packet_read(p, &macroSpecialCount, sizeof(u8));
    while (macroSpecialCount-- > 0) {
        u16 offset;
        packet_read(p, &offset, sizeof(u16));

        s16* respawnInfo = gAreaData[thisAreaIndex].macroObjects + offset;
        packet_read(p, respawnInfo, sizeof(s16));
        LOG_INFO("rx macro special: offset %d, respawnInfo %d", offset, *respawnInfo);

        s32 presetID = (*(respawnInfo - 4) & 0x1FF) - 31;
        const BehaviorScript* behavior = MacroObjectPresets[presetID].behavior;

        struct Object* o = get_object_matching_respawn_info(respawnInfo);
        if (o != NULL) {
            LOG_INFO("rx macro special: object");
            // coin formation
            if (behavior == bhvCoinFormation) {
                o->oBehParams = *respawnInfo;
                o->oCoinUnkF4 = (o->oBehParams >> 8) & 0xFF;

                u8 childIndex = 0;
                for (int i = 0; i < OBJECT_POOL_CAPACITY; i++) {
                    struct Object* o2 = &gObjectPool[i];
                    if (o2->parentObj != o) { continue; }
                    if (o2 == o) { continue; }
                    if (o2->behavior != bhvCoinFormationSpawn && o2->behavior != bhvYellowCoin) { continue; }
                    if (o->oCoinUnkF4 & (1 << childIndex++)) {
                        obj_mark_for_deletion(o2);
                    }
                }
                LOG_INFO("rx macro special: coin formation");
            }
        }
    }
}
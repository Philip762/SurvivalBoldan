#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/string.h>
#include <libdl/hud.h>

#include "utils.h"
#include "maputils.h"
#include "items/item.h"
#include "bonus_bolts.h"

#define ITEM_BONUS_BOLTS_AMOUNT (25000)

int itemBonusBolts_Bought[GAME_MAX_PLAYERS] = {0};

int itemBonusBolts_HasRoomForMore(int defIdx, struct SurvivalItemDef *def, int playerId) {
    if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return 0;
    return !itemBonusBolts_Bought[playerId];
}

void itemBonusBolts_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId) {
    Player *player = playerGetFromIndex(playerId);
    if (!playerIsValid(player) || !MapConfig.State) return;
    if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return;

    itemBonusBolts_Bought[playerId] = 1;

    if (!player->IsLocal) return;

    struct SurvivalPlayer *playerData = &MapConfig.State->PlayerStates[playerId];
    playerData->State.Bolts += ITEM_BONUS_BOLTS_AMOUNT;
    playerData->State.TotalBolts += ITEM_BONUS_BOLTS_AMOUNT;
}

void itemBonusBolts_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemBonusBolts_Bought, 0, sizeof(itemBonusBolts_Bought));
}

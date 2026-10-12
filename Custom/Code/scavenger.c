#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/string.h>

#include "utils.h"
#include "maputils.h"
#include "items/item.h"
#include "mobys/ammodrop.h"
#include "boldan.h"
#include "scavenger.h"

#define ITEM_SCAVENGER_KILLS_REQUIRED (4)
#define ITEM_SCAVENGER_PROBABILITY_PER_LEVEL (0.025)

int itemScavenger_KillCounters[GAME_MAX_PLAYERS] = {0};

void itemScavenger_OnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId) {
    if (killedByPlayerId < 0 || killedByPlayerId >= GAME_MAX_PLAYERS) return;

    Player *player = playerGetFromIndex(killedByPlayerId);
    if (!playerIsValid(player) || !player->IsLocal || !itemCanConsume(player, ITEM_SCAVENGER)) return;

    if (++itemScavenger_KillCounters[killedByPlayerId] < ITEM_SCAVENGER_KILLS_REQUIRED) return;
    itemScavenger_KillCounters[killedByPlayerId] = 0;

    int level = playerGetItemCount(player, ITEM_SCAVENGER);
    if (level <= 0) return;

    float probability = level * ITEM_SCAVENGER_PROBABILITY_PER_LEVEL;
    if (randRange(0, 1) >= probability) return;

    ammodropCreateAt(moby);
}

void itemScavenger_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemScavenger_KillCounters, 0, sizeof(itemScavenger_KillCounters));
}

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
#include "boldan.h"
#include "critical_ascension.h"

#define ITEM_CRITICAL_ASCENSION_CHANCE_PER_LEVEL (0.075)
#define ITEM_CRITICAL_ASCENSION_MAX_CHANCE (1.00)

int itemCriticalAscension_RollUltra(Player *player) {
    int level = playerGetItemCount(player, ITEM_CRITICAL_ASCENSION);
    if (level <= 0) return 0;

    float chance = level * ITEM_CRITICAL_ASCENSION_CHANCE_PER_LEVEL;
    if (chance > ITEM_CRITICAL_ASCENSION_MAX_CHANCE) chance = ITEM_CRITICAL_ASCENSION_MAX_CHANCE;

    return randRange(0, 1) < chance;
}

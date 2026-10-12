#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/moby.h>
#include <libdl/math.h>
#include <libdl/string.h>

#include "utils.h"
#include "maputils.h"
#include "items/item.h"
#include "mobs/mob.h"
#include "boldan.h"
#include "short_fuse.h"

#define ITEM_SHORT_FUSE_INITIAL_CHANCE (0.025)
#define ITEM_SHORT_FUSE_CHAIN_CHANCE (0.85)
#define ITEM_SHORT_FUSE_DAMAGE_PER_LEVEL (20)
#define ITEM_SHORT_FUSE_RADIUS (1.25)
#define ITEM_SHORT_FUSE_KNOCKBACK (0)
#define ITEM_SHORT_FUSE_COLOR_TEAM (TEAM_BLUE)

int itemShortFuse_Chaining[GAME_MAX_PLAYERS] = {0};
int itemShortFuse_Exploding[GAME_MAX_PLAYERS] = {0};

// projectiles the player's weapons spawn, taken from libdl's MobyId enum.
// used to tell a real shot apart from abilities, explosions and mod spawned
// damage, so a proc cannot chain off its own splash. extend if a weapon procs nothing
const int itemShortFuse_WeaponShotOClasses[] = {
    // melee
    MOBY_ID_WRENCH,
    MOBY_ID_FLAIL,
    MOBY_ID_FLAIL_HEAD,
    // dual vipers
    MOBY_ID_DUAL_VIPER_SHOT,
    MOBY_ID_BULLET_SPLASH_VIPERS_MAG_FLAIL,
    // magma cannon, the impact rocks carry the damage on upgraded shots
    MOBY_ID_MAGMA_CANNON,
    MOBY_ID_MAGMA_CANNON_SHOT,
    // arbiter
    MOBY_ID_ARBITER_ROCKET0,
    MOBY_ID_ARBITER_ROCKET1,
    // fusion rifle
    MOBY_ID_FUSION_SHOT,
    // mine launcher
    MOBY_ID_MINE_LAUNCHER_MINE,
    // b6 obliterator, the bomb explosion carries the damage, not the ball
    MOBY_ID_B6_BALL0,
    MOBY_ID_B6_BALL1,
    MOBY_ID_B6_BOMB_EXPLOSION,
    // omni shield
    MOBY_ID_HOLOSHIELD_SHOT
};

static int itemShortFuse_IsWeaponShot(int oclass) {
    int i;
    for (i = 0; i < COUNT_OF(itemShortFuse_WeaponShotOClasses); ++i) {
        if (itemShortFuse_WeaponShotOClasses[i] == oclass) return 1;
    }

    return 0;
}

void itemShortFuse_OnDealtDamage(Player *player, Moby *sourceMoby, Moby *mobMoby) {
    if (!playerIsValid(player) || !player->IsLocal || !mobMoby) return;
    if (!sourceMoby || !itemShortFuse_IsWeaponShot(sourceMoby->OClass)) return;

    int level = playerGetItemCount(player, ITEM_SHORT_FUSE);
    if (level <= 0) return;

    int playerId = player->PlayerId;
    if (itemShortFuse_Exploding[playerId]) return;

    float chance = itemShortFuse_Chaining[playerId] ? ITEM_SHORT_FUSE_CHAIN_CHANCE : ITEM_SHORT_FUSE_INITIAL_CHANCE;
    if (randRange(0, 1) >= chance) {
        itemShortFuse_Chaining[playerId] = 0;
        return;
    }

    itemShortFuse_Chaining[playerId] = 1;
    itemShortFuse_Exploding[playerId] = 1;

    spawnExplosion(mobMoby->Position, ITEM_SHORT_FUSE_RADIUS, TEAM_COLORS[ITEM_SHORT_FUSE_COLOR_TEAM]);
    
    int damage = boldanGetAbilityDamage(player, level * ITEM_SHORT_FUSE_DAMAGE_PER_LEVEL);

    mobReactToExplosionAt(player->PlayerMoby, mobMoby->Position, damage, ITEM_SHORT_FUSE_RADIUS, ITEM_SHORT_FUSE_KNOCKBACK);

    itemShortFuse_Exploding[playerId] = 0;
}

void itemShortFuse_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemShortFuse_Chaining, 0, sizeof(itemShortFuse_Chaining));
    memset(itemShortFuse_Exploding, 0, sizeof(itemShortFuse_Exploding));
}

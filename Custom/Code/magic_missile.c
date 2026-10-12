#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/collision.h>
#include <libdl/math.h>
#include <libdl/string.h>

#include "mobs/mob.h"
#include "items/item.h"
#include "utils.h"
#include "maputils.h"
#include "boldan.h"
#include "magic_missile.h"

#define ITEM_MAGIC_MISSILE_DAMAGE_PER_LEVEL (40)
#define ITEM_MAGIC_MISSILE_SHOT_SPEED (0.7f)
#define ITEM_MAGIC_MISSILE_SHOT_LIFE_SECONDS (2.5f)
#define ITEM_MAGIC_MISSILE_SHOT_TYPE (0)
#define ITEM_MAGIC_MISSILE_SHOT_COLOR_TEAM (TEAM_BLUE)
#define ITEM_MAGIC_MISSILE_SHOT_COUNT (4)
#define ITEM_MAGIC_MISSILE_SHOT_COUNT_PER_LEVEL (2)
#define ITEM_MAGIC_MISSILE_SHOT_SPACING_DEG (3)
#define ITEM_MAGIC_MISSILE_KILLS_REQUIRED (10)
#define ITEM_MAGIC_MISSILE_KILLS_REQUIRED_PER_LEVEL (0)

int itemMagicMissile_KillCounters[GAME_MAX_PLAYERS] = {0};

static int itemMagicMissile_CalculateDamage(Player *player) {
    int damage = playerGetItemCount(player, ITEM_MAGIC_MISSILE) * ITEM_MAGIC_MISSILE_DAMAGE_PER_LEVEL;
    return boldanGetAbilityDamage(player, damage);
}

static void itemMagicMissile_FireShotAt(
    Player *player,
    VECTOR startPosition,
    VECTOR direction,
    int damage,
    int type,
    enum TEAM_IDS color,
    int shotLifeTicks,
    float speed,
    u32 damageFlags
) {
    VECTOR shotDirection;

    vector_normalize(shotDirection, direction);
    vector_scale(shotDirection, shotDirection, speed);

    Moby *shotMoby = ((Moby * (*)(float, float, VECTOR, VECTOR, Moby *, int, int, int, int))0x0045d598)(
        4, damage, startPosition, shotDirection, player->PlayerMoby, 1, 0x222124, -1, 0
    );

    if (!shotMoby) return;

    ((void (*)(Moby *, int))0x0045d758)(shotMoby, type);
    ((void (*)(Moby *, int))0x0045d788)(shotMoby, color);
    ((void (*)(Moby *, int))0x0045d7A8)(shotMoby, damageFlags);
    ((void (*)(Moby *, int))0x0045d798)(shotMoby, shotLifeTicks);

    shotMoby->Bolts = 0;
    shotMoby->PParent = player->PlayerMoby;
}

static void itemMagicMissile_FireShot(
    Player *player, int damage, int type, enum TEAM_IDS color, int shotLifeTicks, float rotationRad, float speed
) {
    VECTOR shotDirection;
    MATRIX rot;

    float yaw = atan2f(player->CameraForward[1], player->CameraForward[0]);
    vector_fromyaw(shotDirection, yaw);

    if (rotationRad != 0) {
        matrix_unit(rot);
        matrix_rotate_z(rot, rot, rotationRad);
        vector_apply(shotDirection, shotDirection, rot);
    }

    VECTOR startPosition;
    vector_copy(startPosition, player->PlayerPosition);
    startPosition[2] += 1.34f; // raise up since PlayerPosition is at the feet of ratchet

    itemMagicMissile_FireShotAt(
        player, startPosition, shotDirection, damage, type, color, shotLifeTicks, speed, MOB_DAMAGE_FLAG_BASE
    );
}

static void itemMagicMissile_shootAllDarts(Player *player, int shotCount, float spacingDeg) {
    int damage = itemMagicMissile_CalculateDamage(player);

    float center = (shotCount - 1) * 0.5f;
    float spacingRad = spacingDeg * MATH_DEG2RAD;

    int i;
    for (i = 0; i < shotCount; i++) {
        float angle = (i - center) * spacingRad;

        itemMagicMissile_FireShot(
            player,
            damage,
            ITEM_MAGIC_MISSILE_SHOT_TYPE,
            ITEM_MAGIC_MISSILE_SHOT_COLOR_TEAM,
            ITEM_MAGIC_MISSILE_SHOT_LIFE_SECONDS * TPS,
            angle,
            ITEM_MAGIC_MISSILE_SHOT_SPEED
        );
    }
}

void itemMagicMissile_OnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId) {
    if (killedByPlayerId < 0 || killedByPlayerId >= GAME_MAX_PLAYERS) return;

    Player *player = playerGetFromIndex(killedByPlayerId);
    if (!playerIsValid(player) || !player->IsLocal || !itemCanConsume(player, ITEM_MAGIC_MISSILE)) return;

    int killsRequired = ITEM_MAGIC_MISSILE_KILLS_REQUIRED - (playerGetItemCount(player, ITEM_MAGIC_MISSILE) - 1) *ITEM_MAGIC_MISSILE_KILLS_REQUIRED_PER_LEVEL;

    itemMagicMissile_KillCounters[killedByPlayerId]++;
    if (itemMagicMissile_KillCounters[killedByPlayerId] >= killsRequired) {
        itemMagicMissile_KillCounters[killedByPlayerId] = 0;
        itemBeginConsume(player->PlayerId, ITEM_MAGIC_MISSILE);
    }
}

void itemMagicMissile_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId) {
    Player *player = playerGetFromIndex(playerId);
    if (!playerIsValid(player)) return;
    if (!player->IsLocal) return;

    int shotCount =
        ITEM_MAGIC_MISSILE_SHOT_COUNT + (playerGetItemCount(player, ITEM_MAGIC_MISSILE) - 1) * ITEM_MAGIC_MISSILE_SHOT_COUNT_PER_LEVEL;

    itemMagicMissile_shootAllDarts(player, shotCount, ITEM_MAGIC_MISSILE_SHOT_SPACING_DEG);
}

void itemMagicMissile_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemMagicMissile_KillCounters, 0, sizeof(itemMagicMissile_KillCounters));
}

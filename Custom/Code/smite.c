#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/math.h>
#include <libdl/moby.h>
#include <libdl/string.h>
#include <libdl/hud.h>

#include "utils.h"
#include "maputils.h"
#include "items/item.h"
#include "mobs/mob.h"
#include "boldan.h"
#include "smite.h"

#define ITEM_SMITE_DURATION_SECONDS (25)
#define ITEM_SMITE_REGEN_PERCENT_PER_LEVEL (0.01)
#define ITEM_SMITE_DAMAGE_PERCENT_PER_LEVEL (0.10)
#define ITEM_SMITE_INITIAL_EXPLOSION_SIZE (4.0)
#define ITEM_SMITE_INITIAL_KNOCKBACK_RADIUS (12.0)
#define ITEM_SMITE_INITIAL_KNOCKBACK_POWER (15)
#define ITEM_SMITE_INITIAL_KNOCKBACK_DAMAGE (100)
#define ITEM_SMITE_COLOR (0x8030A0FF)
#define ITEM_SMITE_ICON_COLOR (0xFFFFFFFF)
#define ITEM_SMITE_ICON_BLINK_HZ (2.0)
#define ITEM_SMITE_ICON_ALPHA_MIN (0x30)
#define ITEM_SMITE_TRIGGER_STATE (PLAYER_STATE_JUMP_ATTACK)
#define ITEM_SMITE_TRIGGER_GROUND_DIST (0.01)

int itemSmite_TicksRemaining[GAME_MAX_PLAYERS] = {0};
int itemSmite_RegenTicker[GAME_MAX_PLAYERS] = {0};
int itemSmite_BlinkTicker = 0;

static float itemSmite_fmodf(float x, float y) {
    float quotient = x / y;
    int truncated = (int)quotient;
    return x - ((float)truncated * y);
}

static int itemSmite_GetLevel(Player *player) {
    return playerGetItemCount(player, ITEM_SMITE);
}

static int itemSmite_IsActive(int playerId) {
    if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return 0;
    return itemSmite_TicksRemaining[playerId] > 0;
}

// Pulse the icon alpha so an active smite is visible at a glance.
static float itemSmite_GetBlinkScale(void) {
    float periodTicks = TPS / ITEM_SMITE_ICON_BLINK_HZ;
    float angle = ((float)itemSmite_BlinkTicker / periodTicks) * MATH_TAU;

    angle = itemSmite_fmodf(angle, MATH_TAU);
    if (angle < 0.0f) {
        angle += MATH_TAU;
    }

    return 0.5f + 0.5f * cosf(angle);
}

void itemSmite_DrawUpdate(int defIdx, struct SurvivalItemDef *def) {
    if (!def) return;

    itemSmite_BlinkTicker++;

    int i;
    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
        Player *player = playerGetFromSlot(i);
        if (!playerIsValid(player)) continue;

        if (!itemSmite_IsActive(player->PlayerId)) continue;

        u32 alpha = (u32)(ITEM_SMITE_ICON_ALPHA_MIN + (255 - ITEM_SMITE_ICON_ALPHA_MIN) * itemSmite_GetBlinkScale());
        def->TexColor = ((u32)ITEM_SMITE_ICON_COLOR & 0x00FFFFFF) | (alpha << 24);
        return;
    }

    def->TexColor = ITEM_SMITE_ICON_COLOR;
}

float itemSmite_GetDamageBuff(Player *player) {
    if (!player) return 0;
    if (!itemSmite_IsActive(player->PlayerId)) return 0;

    return itemSmite_GetLevel(player) * ITEM_SMITE_DAMAGE_PERCENT_PER_LEVEL;
}

void itemSmite_Tick(int defIdx, struct SurvivalItemDef *def) {
    int i;

    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        Player *player = playerGetFromIndex(i);
        if (!playerIsValid(player)) continue;
        if (!player->IsLocal) continue;

        int playerId = player->PlayerId;

        if (itemSmite_GetLevel(player) == 0) continue;
        if (itemSmite_IsActive(playerId)) continue;
        if (!itemCanConsume(player, ITEM_SMITE)) continue;

        if (player->PlayerState != ITEM_SMITE_TRIGGER_STATE) continue;
        if (player->Ground.dist >= ITEM_SMITE_TRIGGER_GROUND_DIST) continue;

        itemBeginConsume(playerId, ITEM_SMITE);
    }

    for (i = 0; i < GAME_MAX_PLAYERS; ++i) {
        if (!itemSmite_IsActive(i)) continue;

        itemSmite_TicksRemaining[i]--;

        Player *player = playerGetFromIndex(i);
        if (!playerIsValid(player)) continue;
        if (!player->IsLocal) continue;

        if (++itemSmite_RegenTicker[i] < TPS) continue;
        itemSmite_RegenTicker[i] = 0;

        if (playerIsDead(player) || player->Health >= player->MaxHealth) continue;

        float healPercent = itemSmite_GetLevel(player) * ITEM_SMITE_REGEN_PERCENT_PER_LEVEL;
        playerSetHealth(player, clamp(player->Health + (player->MaxHealth * healPercent), 0, player->MaxHealth));
    }
}

void itemSmite_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId) {
    Player *player = playerGetFromIndex(playerId);
    if (!playerIsValid(player)) return;
    if (itemSmite_GetLevel(player) <= 0) return;

    itemSmite_TicksRemaining[playerId] = (ITEM_SMITE_DURATION_SECONDS * TPS);
    itemSmite_RegenTicker[playerId] = 0;

    spawnExplosion(player->PlayerPosition, ITEM_SMITE_INITIAL_EXPLOSION_SIZE, ITEM_SMITE_COLOR);

    if (player->IsLocal) {
        int damage = boldanGetAbilityDamage(player, ITEM_SMITE_INITIAL_KNOCKBACK_DAMAGE);

        mobReactToExplosionAt(
            player->PlayerMoby,
            player->PlayerPosition,
            damage,
            ITEM_SMITE_INITIAL_KNOCKBACK_RADIUS,
            ITEM_SMITE_INITIAL_KNOCKBACK_POWER
        );
    }
}

void itemSmite_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemSmite_TicksRemaining, 0, sizeof(itemSmite_TicksRemaining));
    memset(itemSmite_RegenTicker, 0, sizeof(itemSmite_RegenTicker));

    itemSmite_BlinkTicker = 0;
}

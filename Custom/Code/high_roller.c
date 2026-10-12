#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/math.h>
#include <libdl/string.h>
#include <libdl/hud.h>

#include "utils.h"
#include "maputils.h"
#include "items/item.h"
#include "boldan.h"
#include "high_roller.h"

#define ITEM_HIGH_ROLLER_ROLL_INTERVAL_SECONDS (30)
#define ITEM_HIGH_ROLLER_EFFECT_DURATION_SECONDS (30)
#define ITEM_HIGH_ROLLER_ROLL_BONUS_PER_LEVEL_AFTER_FIRST (10)
#define ITEM_HIGH_ROLLER_MAX_ROLL (100)
#define ITEM_HIGH_ROLLER_SNACK_TICKS (TPS * 3.5)

#define ITEM_HIGH_ROLLER_LOSS_MAX (10)
#define ITEM_HIGH_ROLLER_BREAKEVEN_MAX (25)
#define ITEM_HIGH_ROLLER_WIN_MAX (50)
#define ITEM_HIGH_ROLLER_BIGWIN_MAX (75)

#define ITEM_HIGH_ROLLER_LOSS_DAMAGE (-0.05)
#define ITEM_HIGH_ROLLER_LOSS_SPEED (-0.05)
#define ITEM_HIGH_ROLLER_WIN_DAMAGE (0.10)
#define ITEM_HIGH_ROLLER_BIGWIN_DAMAGE (0.20)
#define ITEM_HIGH_ROLLER_SUPERWIN_DAMAGE (0.30)
#define ITEM_HIGH_ROLLER_JACKPOT_DAMAGE (0.50)
#define ITEM_HIGH_ROLLER_SUPERWIN_DAMAGE_REDUCTION (0.05)
#define ITEM_HIGH_ROLLER_JACKPOT_DAMAGE_REDUCTION (0.10)

#define ITEM_HIGH_ROLLER_TEXT_COLOR_LOSS "\x0E"
#define ITEM_HIGH_ROLLER_TEXT_COLOR_WIN "\x0B"
#define ITEM_HIGH_ROLLER_TEXT_COLOR_JACKPOT "\x0D"
#define ITEM_HIGH_ROLLER_TEXT_COLOR_END "\x08"

int itemHighRoller_RollTicker[GAME_MAX_PLAYERS] = {0};
int itemHighRoller_EffectTicks[GAME_MAX_PLAYERS] = {0};
float itemHighRoller_DamageBuff[GAME_MAX_PLAYERS] = {0};
float itemHighRoller_SpeedBuff[GAME_MAX_PLAYERS] = {0};
float itemHighRoller_DamageReductionBuff[GAME_MAX_PLAYERS] = {0};

static const char *itemHighRoller_GetOutcomeLabel(int roll) {
    if (roll <= ITEM_HIGH_ROLLER_LOSS_MAX) return "Loss";
    if (roll <= ITEM_HIGH_ROLLER_BREAKEVEN_MAX) return "Break Even";
    if (roll <= ITEM_HIGH_ROLLER_WIN_MAX) return "Win";
    if (roll <= ITEM_HIGH_ROLLER_BIGWIN_MAX) return "Big Win";
    if (roll < ITEM_HIGH_ROLLER_MAX_ROLL) return "Super Big Win";
    return "Jackpot";
}

// shows what the roll actually grants, whole percents so no float formatting
static int itemHighRoller_AppendEffect(char *buf, int size, int len, float value, const char *label) {
    int pct;
    char sign;

    if (value == 0) return len;

    pct = (int)((value * 100.0f) + (value < 0 ? -0.5f : 0.5f));
    sign = pct < 0 ? '-' : '+';
    if (pct < 0) pct = -pct;

    if (len > 0 && len <= size - 3) {
        buf[len++] = ',';
        buf[len++] = ' ';
        buf[len] = '\0';
    }

    if (len < size) {
        snprintf(buf + len, size - len, "%c%d%% %s", sign, pct, label);
        len = strlen(buf);
    }

    return len;
}

static void itemHighRoller_GetEffectText(char *buf, int size, float damage, float speed, float damageReduction) {
    int len = 0;

    buf[0] = '\0';

    len = itemHighRoller_AppendEffect(buf, size, len, damage, "dmg");
    len = itemHighRoller_AppendEffect(buf, size, len, speed, "spd");
    len = itemHighRoller_AppendEffect(buf, size, len, damageReduction, "dmg reduction");

    if (len == 0) snprintf(buf, size, "no effect");
}

static void itemHighRoller_ApplyOutcome(int playerId, int roll) {
    float damage = 0;
    float speed = 0;
    float damageReduction = 0;
    const char *label = itemHighRoller_GetOutcomeLabel(roll);

    if (roll <= ITEM_HIGH_ROLLER_LOSS_MAX) {
        damage = ITEM_HIGH_ROLLER_LOSS_DAMAGE;
        speed = ITEM_HIGH_ROLLER_LOSS_SPEED;
    } else if (roll <= ITEM_HIGH_ROLLER_BREAKEVEN_MAX) {
        damage = 0;
        speed = 0;
    } else if (roll <= ITEM_HIGH_ROLLER_WIN_MAX) {
        damage = ITEM_HIGH_ROLLER_WIN_DAMAGE;
    } else if (roll <= ITEM_HIGH_ROLLER_BIGWIN_MAX) {
        damage = ITEM_HIGH_ROLLER_BIGWIN_DAMAGE;
    } else if (roll < ITEM_HIGH_ROLLER_MAX_ROLL) {
        damage = ITEM_HIGH_ROLLER_SUPERWIN_DAMAGE;
        damageReduction = ITEM_HIGH_ROLLER_SUPERWIN_DAMAGE_REDUCTION;
    } else {
        damage = ITEM_HIGH_ROLLER_JACKPOT_DAMAGE;
        damageReduction = ITEM_HIGH_ROLLER_JACKPOT_DAMAGE_REDUCTION;
    }

    itemHighRoller_DamageBuff[playerId] = damage;
    itemHighRoller_SpeedBuff[playerId] = speed;
    itemHighRoller_DamageReductionBuff[playerId] = damageReduction;
    itemHighRoller_EffectTicks[playerId] = (ITEM_HIGH_ROLLER_EFFECT_DURATION_SECONDS * TPS);

    Player *player = playerGetFromIndex(playerId);
    if (playerIsValid(player) && player->IsLocal) {
        char effect[48];
        char buf[128];
        const char *colorStart = "";
        const char *colorEnd = "";

        // jackpot first, it pays out a gain so the sign tests would colour it as a win
        if (roll >= ITEM_HIGH_ROLLER_MAX_ROLL) {
            colorStart = ITEM_HIGH_ROLLER_TEXT_COLOR_JACKPOT;
        } else if (damage < 0 || speed < 0) {
            colorStart = ITEM_HIGH_ROLLER_TEXT_COLOR_LOSS;
        } else if (damage > 0 || speed > 0 || damageReduction > 0) {
            colorStart = ITEM_HIGH_ROLLER_TEXT_COLOR_WIN;
        }

        // break even stays default, so it gets no codes at all
        if (colorStart[0] != '\0') colorEnd = ITEM_HIGH_ROLLER_TEXT_COLOR_END;

        itemHighRoller_GetEffectText(effect, sizeof(effect), damage, speed, damageReduction);
        snprintf(buf, sizeof(buf), "High Roller: %s%s%s (%d) %s", colorStart, label, colorEnd, roll, effect);
        pushSnack(player->LocalPlayerIndex, buf, ITEM_HIGH_ROLLER_SNACK_TICKS);
    }
}

float itemHighRoller_GetDamageBuff(Player *player) {
    if (itemHighRoller_EffectTicks[player->PlayerId] <= 0) return 0;
    return itemHighRoller_DamageBuff[player->PlayerId];
}

float itemHighRoller_GetSpeedBuff(Player *player) {
    if (itemHighRoller_EffectTicks[player->PlayerId] <= 0) return 0;
    return itemHighRoller_SpeedBuff[player->PlayerId];
}

float itemHighRoller_GetDamageReductionBuff(Player *player) {
    if (itemHighRoller_EffectTicks[player->PlayerId] <= 0) return 0;
    return itemHighRoller_DamageReductionBuff[player->PlayerId];
}

u32 itemHighRoller_GetConsumeCooldownTicks(int defIdx, struct SurvivalItemDef *def, int playerId) {
    if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return 0;
    if (itemHighRoller_EffectTicks[playerId] <= 0) return 0;

    return (ITEM_HIGH_ROLLER_EFFECT_DURATION_SECONDS * TPS);
}

void itemHighRoller_OnPlayerTick(Player *player) {
    int playerId = player->PlayerId;
    if (playerId < 0 || playerId >= GAME_MAX_PLAYERS) return;

    int level = playerGetItemCount(player, ITEM_HIGH_ROLLER);
    if (level <= 0) {
        itemHighRoller_EffectTicks[playerId] = 0;
        itemHighRoller_DamageBuff[playerId] = 0;
        itemHighRoller_SpeedBuff[playerId] = 0;
        itemHighRoller_DamageReductionBuff[playerId] = 0;
        return;
    }

    if (++itemHighRoller_RollTicker[playerId] >= (ITEM_HIGH_ROLLER_ROLL_INTERVAL_SECONDS * TPS)) {
        itemHighRoller_RollTicker[playerId] = 0;

        int roll = (int)randRange(0, ITEM_HIGH_ROLLER_MAX_ROLL + 1);
        roll += (level - 1) * ITEM_HIGH_ROLLER_ROLL_BONUS_PER_LEVEL_AFTER_FIRST;
        if (roll > ITEM_HIGH_ROLLER_MAX_ROLL) roll = ITEM_HIGH_ROLLER_MAX_ROLL;

        itemHighRoller_ApplyOutcome(playerId, roll);
        itemBeginConsume(playerId, ITEM_HIGH_ROLLER);
    }

    if (itemHighRoller_EffectTicks[playerId] > 0) {
        itemHighRoller_EffectTicks[playerId]--;
        player->Speed *= 1.0f + itemHighRoller_GetSpeedBuff(player);
    }
}

void itemHighRoller_Init(int defIdx, struct SurvivalItemDef *def) {
    memset(itemHighRoller_RollTicker, 0, sizeof(itemHighRoller_RollTicker));
    memset(itemHighRoller_EffectTicks, 0, sizeof(itemHighRoller_EffectTicks));
    memset(itemHighRoller_DamageBuff, 0, sizeof(itemHighRoller_DamageBuff));
    memset(itemHighRoller_SpeedBuff, 0, sizeof(itemHighRoller_SpeedBuff));
    memset(itemHighRoller_DamageReductionBuff, 0, sizeof(itemHighRoller_DamageReductionBuff));
}

#include <libdl/dialog.h>
#include <libdl/stdio.h>
#include <libdl/utils.h>
#include <libdl/random.h>
#include <libdl/player.h>
#include <libdl/weapon.h>
#include <libdl/game.h>
#include <libdl/gamesettings.h>
#include <libdl/math.h>
#include <libdl/string.h>
#include <libdl/hud.h>
#include <libdl/area.h>
#include <libdl/spawnpoint.h>
#include <libdl/moby.h>
#include <libdl/ui.h>
#include <libdl/pad.h>

#include "utils.h"
#include "maputils.h"
#include "game.h"
#include "gate.h"
#include "mobs/mob.h"
#include "items/item.h"
#include "boldan.h"
#include "critical_ascension.h"
#include "high_roller.h"
#include "magic_missile.h"
#include "scavenger.h"
#include "short_fuse.h"
#include "smite.h"

int mapOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId);
void mapOnPlayerUpdate(Player *player);
void mapReturnPlayersToMap(void);
int mapGetRoundTransitionTime(int round);
void boldan_RoundRushTick(void);

#define MOB_HEALTH_PLAYER_COUNT_FACTOR (0.05)

#define BOLDAN_DARK_TREMOR_IDX (6)
#define BOLDAN_DARK_TREMOR_START_ROUND (50)
#define BOLDAN_DARK_TREMOR_NORMAL_PROBABILITY (0.015)

#define BOLDAN_OOB_AREA_INDEX_START (1)
#define BOLDAN_OOB_AREA_INDEX_END (5)
#define BOLDAN_OOB_HEALTH_LOSS (0.33)

#define MOB_ZOMBIE_IDX (7)

#define BOLDAN_ARSENAL_WEAPON_SLOT_MIN (WEAPON_SLOT_VIPERS)
#define BOLDAN_ARSENAL_WEAPON_SLOT_MAX (WEAPON_SLOT_COUNT - 1)
#define BOLDAN_ARSENAL_SNACK_TICKS (TPS * 3)

#define BOLDAN_ROUND_RUSH_START_ROUND (75)
#define BOLDAN_HEADSTART_UPGRADE_COOLDOWN_TICKS (4)

#define BOLDAN_MAX_SPEED_BONUS_START_ROUND (75)
#define BOLDAN_MAX_SPEED_BONUS_INTERVAL_ROUNDS (25)
#define BOLDAN_MAX_SPEED_BONUS (0.125)

static float boldan_GetDifficultyAtRound(int round) {
    if (!MapConfig.State) return 0;

	float difficultyBySpawnedMobs = MapConfig.State->MobStats.TotalSpawned / 100;
	float difficultyBump = 0.6 + powf(1.5, round / 25) * 0.4;
	float difficulty = difficultyBySpawnedMobs * difficultyBump * MapConfig.State->Difficulty;
	return difficulty;
}

static float boldan_GetCurrentDifficulty(void) {
	if (!MapConfig.State) return 0;
	return boldan_GetDifficultyAtRound(MapConfig.State->RoundNumber);
}

static u16 boldan_GetBangles(struct MobConfig *config, int spawnParamsIdx) {
    if (spawnParamsIdx != MOB_ZOMBIE_IDX || MOB_ZOMBIE_IDX < 0) return (u16)config->Bangles;

    static const u16 allowedHeads[] = {ZOMBIE_BANGLE_HEAD_1, ZOMBIE_BANGLE_HEAD_2, ZOMBIE_BANGLE_HEAD_3, ZOMBIE_BANGLE_HEAD_5};
    static const int allowedHeadsCount = COUNT_OF(allowedHeads);

    static const u16 allowedTorsos[] = {ZOMBIE_BANGLE_TORSO_1, ZOMBIE_BANGLE_TORSO_2, ZOMBIE_BANGLE_TORSO_3};
    static const int allowedTorsosCount = COUNT_OF(allowedTorsos);

    int headIdx = randRangeInt(0, allowedHeadsCount - 1);
    int torsoIdx = randRangeInt(0, allowedTorsosCount - 1);

    return allowedHeads[headIdx] | allowedTorsos[torsoIdx];
}

static void boldan_PopulateSpawnArgsFromConfig(
    struct MobSpawnEventArgs *output, struct MobConfig *config, int spawnParamsIdx, int isBaseConfig, int spawnFlags
) {
    GameSettings *gs = gameGetSettings();
    if (!gs) return;

    float damage = config->Damage;
    float speed = config->Speed;
    float health = config->Health * (1 + (MOB_HEALTH_PLAYER_COUNT_FACTOR * (gs->PlayerCount - 1)));
    float difficulty = boldan_GetCurrentDifficulty();

	// Ensure mobs with a minimum round start at their base speed when spawning for the first time
	struct MobSpawnParams *spawnParams = &MapConfig.DefaultSpawnParams[spawnParamsIdx];
	float difficultyFirstRound = boldan_GetDifficultyAtRound(spawnParams->MinRound);
	float speedFactor = difficulty - difficultyFirstRound;

    if (isBaseConfig) {
        damage = damage * (1 + (MOB_BASE_DAMAGE_SCALE * config->DamageScale * difficulty));
		speed = speed * (1 + (MOB_BASE_SPEED_SCALE * config->SpeedScale * speedFactor));
        health = health * powf(1 + (MOB_BASE_HEALTH_SCALE * config->HealthScale * difficulty), 2);
    }

    float maxSpeed = config->MaxSpeed;
    if (maxSpeed > 0 && MapConfig.State && MapConfig.State->RoundNumber >= BOLDAN_MAX_SPEED_BONUS_START_ROUND) {
        int speedBonusSteps = (MapConfig.State->RoundNumber - BOLDAN_MAX_SPEED_BONUS_START_ROUND) / BOLDAN_MAX_SPEED_BONUS_INTERVAL_ROUNDS + 1;
        maxSpeed += BOLDAN_MAX_SPEED_BONUS * speedBonusSteps;
    }

    if (config->MaxDamage > 0 && damage > config->MaxDamage) damage = config->MaxDamage;
    if (maxSpeed > 0 && speed > maxSpeed) speed = maxSpeed;
    if (config->MaxHealth > 0 && health > config->MaxHealth) health = config->MaxHealth;

    output->SpawnParamsIdx = spawnParamsIdx;
    output->Bolts = config->Bolts;
    output->Xp = config->Xp;
    output->StartHealth = health;
    output->Bangles = boldan_GetBangles(config, spawnParamsIdx);
    output->Damage = (u16)damage;
    output->AttackRadiusEighths = (u8)(config->AttackRadius * 8);
    output->HitRadiusEighths = (u8)(config->HitRadius * 8);
    output->CollRadiusEighths = (u8)(config->CollRadius * 8);
    output->SpeedEighths = (u16)(speed * 8);
    output->ReactionTickCount = (u8)config->ReactionTickCount;
    output->AttackCooldownTickCount = (u16)config->AttackCooldownTickCount;
    output->DamageCooldownTickCount = (u16)config->DamageCooldownTickCount;
    output->MobAttribute = config->MobAttribute;
    output->Behavior = config->Behavior;
}

float boldan_DarkTremorProbability = 0;

static void boldan_InitDarkTremor(void) {
    boldan_DarkTremorProbability = MapConfig.DefaultSpawnParams[BOLDAN_DARK_TREMOR_IDX].Probability;
}

static void boldan_ApplyDarkTremorSpawnRules(void) {
    struct MobSpawnParams *params = &MapConfig.DefaultSpawnParams[BOLDAN_DARK_TREMOR_IDX];

    if (MapConfig.State->RoundIsSpecial || (MapConfig.State->RoundNumber + 1) < BOLDAN_DARK_TREMOR_START_ROUND) {
        params->SpecialRoundOnly = 1;
        params->Probability = boldan_DarkTremorProbability;
        return;
    }

    params->SpecialRoundOnly = 0;
    params->Probability = BOLDAN_DARK_TREMOR_NORMAL_PROBABILITY;
}

int boldanRollCrit(Player *player) {
    float critProbability = playerGetItemCount(player, ITEM_IMMEDIATE_PLAYER_CRIT_UPGRADE) * BOLDAN_CRIT_CHANCE_PER_LEVEL;
    if (randRange(0, 1) >= critProbability) return BOLDAN_CRIT_NONE;
    if (itemCriticalAscension_RollUltra(player)) return BOLDAN_CRIT_ULTRA;
    return BOLDAN_CRIT_NORMAL;
}

float boldanGetItemDamageFactor(Player *player) {
    return itemSmite_GetDamageBuff(player) + itemHighRoller_GetDamageBuff(player);
}

float boldanGetDamageReductionFactor(Player *player) {
    float factor = itemHighRoller_GetDamageReductionBuff(player);

    return clamp(factor, 0, BOLDAN_MAX_DAMAGE_REDUCTION);
}

static void boldan_OnPlayerDecrementHealth(Player *player, float amount) {
    float finalAmount = amount * (1.0f - boldanGetDamageReductionFactor(player));
    if (finalAmount <= 0) return;

    playerDecHealth(player, finalAmount);
}

void boldanApplyPlayerDamage(Player *player, float *damage) {
    int multishotCount = playerGetItemCount(player, ITEM_PASSIVE_EXTRA_SHOT);
    if (multishotCount > 0) *damage *= 1 + multishotCount;

    *damage *= 1 + boldanGetItemDamageFactor(player);
}

float boldanGetCritFactor(Player *player, u32 *damageFlags) {
    int critResult = boldanRollCrit(player);
    if (critResult == BOLDAN_CRIT_NONE) return 0;

    if (damageFlags) *damageFlags |= BOLDAN_DAMAGE_FLAG_CRIT;

    if (critResult == BOLDAN_CRIT_ULTRA) {
        if (damageFlags) *damageFlags |= BOLDAN_DAMAGE_FLAG_ULTRA_CRIT;
        return BOLDAN_ULTRA_CRIT_DAMAGE_FACTOR;
    }

    return BOLDAN_CRIT_DAMAGE_FACTOR;
}

void boldanApplyDamage(Player *player, float *damage, u32 *damageFlags) {
    if (!player) return;

    boldanApplyPlayerDamage(player, damage);

    if (player->IsLocal) {
        *damage *= 1 + boldanGetCritFactor(player, damageFlags);
        *damage *= 1 + (BOLDAN_DAMAGE_UPGRADE_FACTOR * playerGetItemCount(player, ITEM_IMMEDIATE_PLAYER_DAMAGE_UPGRADE));
    }
}

int boldanGetAbilityDamage(Player *player, int baseDamage) {
    float damage = baseDamage;
    boldanApplyDamage(player, &damage, NULL);
    return (int)damage;
}

int boldanOnBeforeDamageMob(Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args) {
    float damage = args->DamageQuarters / 4.0;

    boldanApplyDamage(player, &damage, &args->DamageFlags);

    args->DamageQuarters = (u32)(damage * 4);

    itemShortFuse_OnDealtDamage(player, sourceMoby, mobMoby);

    return 1;
}

static void boldanMobAfterDamage(Moby *moby, struct MobDamageEventArgs *args, float appliedDamage) {
    if (appliedDamage <= 0) return;
    if (!MapConfig.Functions.ModePushBubbleFunc) return;

    struct MobPVar *pvars = (struct MobPVar *)moby->PVar;
    if (!pvars) return;

    VECTOR mobCenter = {0, 0, pvars->TargetVars.targetHeight, 0};
    vector_add(mobCenter, mobCenter, moby->Position);

    Player *damager = playerGetFromUID(args->SourceUID);
    int isLocal = damager ? damager->IsLocal : 0;

    const u32 ultraMask = BOLDAN_DAMAGE_FLAG_CRIT | BOLDAN_DAMAGE_FLAG_ULTRA_CRIT;

    int team = TEAM_YELLOW;
    if ((args->DamageFlags & ultraMask) == ultraMask)
        team = TEAM_PURPLE;
    else if (args->DamageFlags & BOLDAN_DAMAGE_FLAG_CRIT)
        team = TEAM_RED;

    MapConfig.Functions.ModePushBubbleFunc(mobCenter, pvars->MobVars.Config.CollRadius, appliedDamage, isLocal, team);
}

void boldanOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId) {
    mapOnMobKilled(moby, killedByPlayerId, killedByWeaponId);
    itemScavenger_OnMobKilled(moby, killedByPlayerId, killedByWeaponId);
    itemMagicMissile_OnMobKilled(moby, killedByPlayerId, killedByWeaponId);
}

void boldanOnPlayerTick(Player *player) {
    boldan_RoundRushTick();

    if (!MapConfig.State || !MapConfig.ClientsReady) return;

    mapOnPlayerUpdate(player);

    itemHighRoller_OnPlayerTick(player);
}

short boldan_RespawnCuboid[GAME_MAX_PLAYERS];

static short boldan_GetRespawnCuboidIdx(Player *player) {
	Area_t area;
	int areaIdx;

	for (areaIdx = BOLDAN_OOB_AREA_INDEX_START; areaIdx <= BOLDAN_OOB_AREA_INDEX_END; ++areaIdx) {
		if (!areaGetArea(areaIdx, &area)) continue;
		if (!area.Cuboids || area.CuboidCount <= 1) continue;

		int cuboidIdx;
		for (cuboidIdx = 1; cuboidIdx < area.CuboidCount; ++cuboidIdx) {
			SpawnPoint *boxCuboid = spawnPointGet(area.Cuboids[cuboidIdx]);
			if (spawnPointIsPointInside(boxCuboid, player->PlayerPosition, NULL)) return (short)area.Cuboids[0];
		}
	}

	return -1;
}

static void boldan_ReturnPlayersToMap(void) {
	float deathHeight = maxf(1, gameGetDeathHeight()) + 1;
	int i;

	for (i = 0; i < GAME_MAX_LOCALS; ++i) {
		Player *player = playerGetFromSlot(i);
		if (!playerIsValid(player)) continue;

		int playerId = player->PlayerId;
		short cuboidIdx = boldan_GetRespawnCuboidIdx(player);
		boldan_RespawnCuboid[playerId] = cuboidIdx;

		if (player->PlayerPosition[2] >= deathHeight) continue;

		VECTOR p, r, o;

		if (cuboidIdx >= 0) {
			SpawnPoint *sp = spawnPointGet(cuboidIdx);
			vector_copy(p, &sp->M0[12]);
			vector_copy(r, &sp->M1[12]);
		} else {
			playerGetSpawnpoint(player, p, r, 0);
			vector_fromyaw(o, (playerId / (float)GAME_MAX_PLAYERS) * MATH_TAU - MATH_PI);
			vector_scale(o, o, 2.5);
			vector_add(p, p, o);
		}

		playerSetPosRot(player, p, r);
		playerSetHealth(player, maxf(0, player->Health - player->MaxHealth * BOLDAN_OOB_HEALTH_LOSS));
	}
}

static int boldan_GetResurrectPoint(Player *player, VECTOR outPos, VECTOR outRot, int firstRes) {
	if (firstRes) return 0;
	if (!player->IsLocal) return 0;

	int playerId = player->PlayerId;
	if (boldan_RespawnCuboid[playerId] >= 0) {
		SpawnPoint *sp = spawnPointGet(boldan_RespawnCuboid[playerId]);
		if (!sp) return 0;

		vector_copy(outPos, &sp->M0[12]);
		vector_copy(outRot, &sp->M1[12]);
		return 1;
	}

	return 0;
}

char gambitsRandomWeapon[GAME_MAX_LOCALS] = {0};

void gambitsUpYourArsenalOnRoundComplete(int roundNumber) {
    char buf[64];
    int i;

    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
        int weaponId = weaponSlotToId(randRangeInt(BOLDAN_ARSENAL_WEAPON_SLOT_MIN, BOLDAN_ARSENAL_WEAPON_SLOT_MAX));
        gambitsRandomWeapon[i] = weaponId;

        Player *player = playerGetFromSlot(i);
        if (!playerIsValid(player)) continue;

        if (player->GadgetBox->Gadgets[weaponId].Level < 0) {
            playerGiveWeapon(player->GadgetBox, weaponId, 0, 1);
        } else {
            player->GadgetBox->Gadgets[weaponId].Ammo = playerGetWeaponMaxAmmo(player->GadgetBox, weaponId);
        }

        snprintf(buf, sizeof(buf), "New Weapon \x0E%s\x08", uiMsgString(weaponGetDef(weaponId, 0)->basicQSTag));
        pushSnack(i, buf, BOLDAN_ARSENAL_SNACK_TICKS);
    }
}

void gambitsUpYourArsenalTick(void) {
    int i;

    for (i = 0; i < GAME_MAX_LOCALS; ++i) {
        if (gambitsRandomWeapon[i]) {
            mapLocalPlayerEnforceSingleWeaponRestriction(i, gambitsRandomWeapon[i], 0);
        }
    }
}

void gambitsUpYourArsenalInit(void) {
    gambitsUpYourArsenalOnRoundComplete(0);
}

static int boldan_RoundRushState = 0;
static MapPickupUpgradePickup_func boldan_BasePickupUpgrade = NULL;

static void boldan_PickupUpgrade(Moby *moby, int pickedUpByPlayerId) {
    if (boldan_BasePickupUpgrade)
        boldan_BasePickupUpgrade(moby, pickedUpByPlayerId);

    if (!MapConfig.State) return;
    if (pickedUpByPlayerId < 0 || pickedUpByPlayerId >= GAME_MAX_PLAYERS) return;

    MapConfig.State->PlayerStates[pickedUpByPlayerId].ActionCooldownTicks = BOLDAN_HEADSTART_UPGRADE_COOLDOWN_TICKS;
}

static void boldan_RoundRushEndUpgradeDiscount(void) {
    if (!boldan_BasePickupUpgrade) return;

    MapConfig.Functions.PickupUpgradeFunc = boldan_BasePickupUpgrade;
    boldan_BasePickupUpgrade = NULL;
}

static int boldan_GetRoundTransitionTime(int round) {
    if (boldan_RoundRushState == 2 || boldan_RoundRushState == 3) return -1;

    return mapGetRoundTransitionTime(round);
}

void boldan_RoundRushInit(void) {
    MapConfig.Functions.GetRoundTransitionTimeFunc = &boldan_GetRoundTransitionTime;

    boldan_BasePickupUpgrade = MapConfig.Functions.PickupUpgradeFunc;
    MapConfig.Functions.PickupUpgradeFunc = &boldan_PickupUpgrade;

    boldan_RoundRushState = 1;
}

void boldan_RoundRushTick(void) {
    struct SurvivalState *state = MapConfig.State;
    if (!state) return;
    if (boldan_RoundRushState == 0 || boldan_RoundRushState == 4) return;

    if (boldan_RoundRushState == 1) {
        state->RoundNumber = BOLDAN_ROUND_RUSH_START_ROUND - 2;
        state->RoundMaxMobCount = 0;

        int i;
        for (i = 0; i < state->RoundNumber; ++i) {
            state->MobStats.TotalSpawned += MAX_MOBS_BASE + (int)(MAX_MOBS_ROUND_WEIGHT * (1 + i));
        }

        boldan_RoundRushState = 2;
        return;
    }

    if (boldan_RoundRushState == 2) {
        if (!state->RoundMaxMobCount) return;

        state->MobStats.TotalSpawnedThisRound = state->RoundMaxMobCount;
        boldan_RoundRushState = 3;
        return;
    }

    if (state->RoundNumber >= BOLDAN_ROUND_RUSH_START_ROUND) {
        boldan_RoundRushEndUpgradeDiscount();
        boldan_RoundRushState = 4;
    }
}

void boldanTick(void) {
	static int isModeInitialized = 0;
	if (!isModeInitialized && MapConfig.ClientsReady) {
		isModeInitialized = 1;
		MapConfig.Functions.ModePopulateSpawnArgsFunc = &boldan_PopulateSpawnArgsFromConfig;
		boldan_InitDarkTremor();
	}

	if (!isModeInitialized || !MapConfig.State) return;

	boldan_ApplyDarkTremorSpawnRules();
}

void boldanInit(void) {
    int i;
    for (i = 0; i < GAME_MAX_PLAYERS; ++i) boldan_RespawnCuboid[i] = -1;

    weaponGetDef(WEAPON_ID_MINE_LAUNCHER, 0)->mpMaxAmmo = 15;

    MapConfig.Functions.OnMobKilledFunc = &boldanOnMobKilled;
    MapConfig.Functions.OnPlayerUpdateFunc = &boldanOnPlayerTick;
    MapConfig.Functions.OnBeforeDamageMobFunc = &boldanOnBeforeDamageMob;
    MapConfig.Functions.MobAfterDamageFunc = &boldanMobAfterDamage;
    MapConfig.Functions.OnPlayerGetResFunc = &boldan_GetResurrectPoint;

    HOOK_J_OP(&mapReturnPlayersToMap, &boldan_ReturnPlayersToMap, 0);

    HOOK_JAL(0x00605820, &boldan_OnPlayerDecrementHealth);
    POKE_U32(0x005E1870, 0);
}

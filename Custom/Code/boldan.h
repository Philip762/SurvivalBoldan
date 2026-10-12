#ifndef BOLDAN_H
#define BOLDAN_H

#define BOLDAN_CRIT_NONE (0)
#define BOLDAN_CRIT_NORMAL (1)
#define BOLDAN_CRIT_ULTRA (2)

#define BOLDAN_DAMAGE_UPGRADE_FACTOR (0.08)
#define BOLDAN_CRIT_CHANCE_PER_LEVEL (0.01)
#define BOLDAN_CRIT_DAMAGE_FACTOR (2)
#define BOLDAN_ULTRA_CRIT_DAMAGE_FACTOR (3)

#define BOLDAN_DAMAGE_FLAG_CRIT (0x20000000)
#define BOLDAN_DAMAGE_FLAG_ULTRA_CRIT (0x10000000)

#define BOLDAN_MAX_DAMAGE_REDUCTION (0.90)

int boldanRollCrit(Player *player);
int boldanGetAbilityDamage(Player *player, int baseDamage);
float boldanGetItemDamageFactor(Player *player);
float boldanGetDamageReductionFactor(Player *player);
float boldanGetCritFactor(Player *player, u32 *damageFlags);
void boldanApplyPlayerDamage(Player *player, float *damage);
void boldanApplyDamage(Player *player, float *damage, u32 *damageFlags);

int boldanOnBeforeDamageMob(Player *player, Moby *sourceMoby, Moby *mobMoby, struct MobDamageEventArgs *args);
void boldanOnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId);
void boldanOnPlayerTick(Player *player);
void boldanInit(void);
void boldanTick(void);

void gambitsUpYourArsenalInit(void);
void gambitsUpYourArsenalTick(void);
void gambitsUpYourArsenalOnRoundComplete(int roundNumber);

void boldan_RoundRushInit(void);
void boldan_RoundRushTick(void);

#endif

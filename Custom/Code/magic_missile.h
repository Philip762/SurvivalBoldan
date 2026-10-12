#ifndef ITEM_MAGIC_MISSILE_H
#define ITEM_MAGIC_MISSILE_H

void itemMagicMissile_OnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId);

void itemMagicMissile_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId);
void itemMagicMissile_Init(int defIdx, struct SurvivalItemDef *def);

#endif

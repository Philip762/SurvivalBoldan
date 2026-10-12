#ifndef ITEM_SCAVENGER_H
#define ITEM_SCAVENGER_H

void itemScavenger_OnMobKilled(Moby *moby, int killedByPlayerId, int killedByWeaponId);

void itemScavenger_Init(int defIdx, struct SurvivalItemDef *def);

#endif

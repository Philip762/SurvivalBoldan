#ifndef ITEM_HIGH_ROLLER_H
#define ITEM_HIGH_ROLLER_H

float itemHighRoller_GetDamageBuff(Player *player);
float itemHighRoller_GetSpeedBuff(Player *player);
float itemHighRoller_GetDamageReductionBuff(Player *player);
u32 itemHighRoller_GetConsumeCooldownTicks(int defIdx, struct SurvivalItemDef *def, int playerId);

void itemHighRoller_OnPlayerTick(Player *player);
void itemHighRoller_Init(int defIdx, struct SurvivalItemDef *def);

#endif

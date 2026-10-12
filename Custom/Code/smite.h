#ifndef ITEM_SMITE_H
#define ITEM_SMITE_H

float itemSmite_GetDamageBuff(Player *player);

void itemSmite_Tick(int defIdx, struct SurvivalItemDef *def);
void itemSmite_DrawUpdate(int defIdx, struct SurvivalItemDef *def);
void itemSmite_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId);
void itemSmite_Init(int defIdx, struct SurvivalItemDef *def);

#endif

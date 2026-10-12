#ifndef ITEM_BONUS_BOLTS_H
#define ITEM_BONUS_BOLTS_H

int itemBonusBolts_HasRoomForMore(int defIdx, struct SurvivalItemDef *def, int playerId);

void itemBonusBolts_OnConsumed(int defIdx, struct SurvivalItemDef *def, int playerId);
void itemBonusBolts_Init(int defIdx, struct SurvivalItemDef *def);

#endif

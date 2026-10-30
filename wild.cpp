#include "wild.h"
#include "pet.h"     // MAX_LEVEL, speciesShowable()

static uint8_t variantBonus(uint8_t bonus) {
  return bonus > WILD_VARIANT_BONUS_MAX ? WILD_VARIANT_BONUS_MAX : bonus;
}

uint32_t wildVariantThreshold(uint8_t bonus) {
  return WILD_VARIANT_BASE + (uint32_t)variantBonus(bonus) * WILD_VARIANT_PER_POINT;
}

bool wildVariantForRoll(uint32_t roll, uint8_t bonus) {
  return (roll % WILD_VARIANT_SCALE) < wildVariantThreshold(bonus);
}

uint8_t wildLevelMin(uint8_t playerLevel, bool hard) {
  if (hard) return 1;
  return playerLevel > WILD_LEVEL_SPREAD
             ? (uint8_t)(playerLevel - WILD_LEVEL_SPREAD) : 1;
}

uint8_t wildLevelMax(uint8_t playerLevel, bool hard) {
  if (hard) return MAX_LEVEL;
  uint16_t top = (uint16_t)playerLevel + WILD_LEVEL_SPREAD;
  return top > MAX_LEVEL ? MAX_LEVEL : (uint8_t)top;
}

uint8_t wildDropCount(bool hard, uint8_t bonusRoll) {
  return (uint8_t)((hard ? 2 : 1) +
                   (bonusRoll < WILD_BONUS_DROP_CHANCE ? 1 : 0));
}

uint8_t wildEscapeChance(uint8_t playerLevel, uint8_t foeLevel) {
  if (!foeLevel || playerLevel >= foeLevel) return 90;
  uint8_t chance = (uint8_t)((uint16_t)90 * playerLevel / foeLevel);
  return chance < 10 ? 10 : chance;
}

uint8_t wildFoeEscapeChance(uint16_t hp, uint16_t maxHp) {
  if (!maxHp || !hp) return 0;
  if (hp > maxHp) hp = maxHp;
  uint32_t scaled = (uint32_t)hp * 100U;
  if (scaled <= (uint32_t)maxHp * 10U) return 10;
  if (scaled <= (uint32_t)maxHp * 20U) {
    uint32_t above = scaled - (uint32_t)maxHp * 10U;
    return (uint8_t)(10U + above * 10U / ((uint32_t)maxHp * 10U));
  }
  if (scaled <= (uint32_t)maxHp * 40U) {
    uint32_t below = (uint32_t)maxHp * 40U - scaled;
    return (uint8_t)(10U + below * 10U / ((uint32_t)maxHp * 20U));
  }
  return 0;
}

uint8_t wildStageForLevel(uint8_t level) {
  uint8_t st = TS_ST_IN_TRAINING_1;
  for (uint8_t i = 0; i < STAGE_COUNT; i++)
    if (level >= TS_STAGE_ENTRY[i]) st = i;
  return st;
}

uint8_t wildStageFor(uint8_t level, uint8_t roll) {
  uint8_t top = wildStageForLevel(level);
  // Mostly the stage a Digimon of this level would have reached; a quarter of
  // the time one stage behind. Never an In-Training I once Rookies exist: a wild
  // Botamon is no fight at all.
  uint8_t st = (roll < 25 && top > TS_ST_IN_TRAINING_1) ? (uint8_t)(top - 1) : top;
  if (st == TS_ST_IN_TRAINING_1 && top >= TS_ST_ROOKIE) st = TS_ST_IN_TRAINING_2;
  return st;
}

int16_t wildPickSpecies(uint8_t stage, uint32_t roll) {
  uint16_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++)
    if (TS_DEX_TBL[d].tier == stage && speciesShowable(d)) n++;
  if (!n) return 0;
  uint16_t want = (uint16_t)(roll % n);
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (TS_DEX_TBL[d].tier != stage || !speciesShowable(d)) continue;
    if (!want) return d;
    want--;
  }
  return 0;
}

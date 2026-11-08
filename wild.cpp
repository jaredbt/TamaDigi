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

uint8_t wildLevelFor(uint8_t playerLevel) {
  if (!playerLevel) return 1;
  return playerLevel > MAX_LEVEL ? MAX_LEVEL : playerLevel;
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

uint8_t wildStageFor(uint8_t level, bool megaPlusPartner, uint8_t roll) {
  uint8_t st = wildStageForLevel(level);
  const uint8_t top = megaPlusPartner ? TS_ST_MEGA_PLUS : TS_ST_MEGA;
  if (st > top) st = top;
  roll %= 100;
  if (roll < WILD_STAGE_SAME_PCT) return st;
  if (roll < WILD_STAGE_SAME_PCT + WILD_STAGE_BEHIND_PCT)
    return st > TS_ST_IN_TRAINING_1 ? (uint8_t)(st - 1) : st;
  return st < top ? (uint8_t)(st + 1) : st;   // ahead, never past the top
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

int16_t wildJogressPick(const int16_t *halves, uint8_t n, uint8_t roll, uint32_t pick) {
  if (!halves || !n || roll % 100 >= WILD_JOGRESS_PCT) return 0;
  return halves[pick % n];
}

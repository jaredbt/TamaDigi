#pragma once
#include <stdint.h>
#include "ts_dex.h"
#include "ts_skills.h"

// The engine's view of the generated Time Stranger tables. ts_dex.h and
// ts_skills.h are data; the short names and the few rules the rest of the
// firmware shares live here, so each has one home.
#define DEX_COUNT TS_DEX_COUNT
#define MOVE_COUNT TS_SKILL_COUNT         // skill ids run 1 .. MOVE_COUNT - 1
#define STAGE_COUNT TS_RAISABLE_STAGES

// THE one scale constant between Time Stranger's raw numbers and this game's, in
// percent. Stats and evolution minimums both go through tsScaled(), so balance
// can be tuned here without regenerating data.
#define TS_STAT_SCALE_PCT 10
#define TS_GROWTH_LEVEL_MAX 99            // Time Stranger's max; level 100 adds nothing

static inline uint16_t tsScaled(uint32_t raw) {
  return (uint16_t)(raw * TS_STAT_SCALE_PCT / 100);
}

static inline bool dexValid(int16_t d) { return d >= 1 && d <= DEX_COUNT; }

// The raw stat at this level: linear from the level-1 value to the level-99 value.
static inline uint16_t tsGrowth(int16_t dex, uint8_t stat, uint8_t level) {
  if (!dexValid(dex) || stat >= TS_STAT_COUNT) return 0;
  const TsDexEntry &d = TS_DEX_TBL[dex];
  const uint8_t l = level < 1 ? 1 : (level > TS_GROWTH_LEVEL_MAX ? TS_GROWTH_LEVEL_MAX : level);
  return (uint16_t)(d.base[stat] +
                    ((int32_t)d.max[stat] - d.base[stat]) * (l - 1) / (TS_GROWTH_LEVEL_MAX - 1));
}

// ---------- skills ----------

// The basic attack: Time Stranger's Attack command. No slot holds it and no table
// lists it, so every Digimon can always end a fight -- Punimon and Botamon hatch
// with a support skill and nothing else. It is one past the table so that 0 can
// keep meaning "no action" (an item used, a failed escape).
#define SKILL_BASIC ((uint16_t)MOVE_COUNT)
static const TsSkill SKILL_BASIC_ENTRY = {
  "ATTACK", TS_EL_NULL, TS_SC_PHYS, { 30, 30, 30 }, 1, 100, TS_EF_NONE, 0, 0, 0, 0
};

static inline bool skillValid(uint16_t mv) { return mv >= 1 && mv < MOVE_COUNT; }
static inline const TsSkill &skillEntry(uint16_t mv) {
  if (mv == SKILL_BASIC) return SKILL_BASIC_ENTRY;
  return TS_SKILL_TBL[mv < MOVE_COUNT ? mv : 0];
}
static inline const char *moveName(uint16_t mv) { return skillEntry(mv).name; }

// The level bands of the learnable skills: tier I below 25, II at 25-60, III from
// 61. Signatures hold one value three times, so every skill can ask this.
static inline uint8_t skillTier(uint8_t level) { return level < 25 ? 0 : (level <= 60 ? 1 : 2); }
static inline uint8_t skillPower(uint16_t mv, uint8_t level) {
  return skillEntry(mv).power[skillTier(level)];
}

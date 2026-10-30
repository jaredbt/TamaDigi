#pragma once
#include <stdint.h>
#include "roster.h"

// Element and attribute matchups, Time Stranger's. Every Digimon has its own
// resistance row (TsDexEntry::resist), on top of the Vaccine > Virus > Data >
// Vaccine triangle.
//
// Everything is integer percentages: 100 = neutral, 0 = immune (Time Stranger has
// real immunities, 74 cells of the table). No floats anywhere on the MCU.

#define ATTR_STRONG_PCT 130
#define ATTR_WEAK_PCT 80
#define SAME_ELEMENT_NUM 3     // the same-element bonus, as a fraction: 3/2
#define SAME_ELEMENT_DEN 2

// Free, Variable, Unknown and No Data are neutral both ways.
static inline uint16_t attrEffPct(uint8_t atk, uint8_t def) {
  if ((atk == TS_AT_VACCINE && def == TS_AT_VIRUS) || (atk == TS_AT_VIRUS && def == TS_AT_DATA) ||
      (atk == TS_AT_DATA && def == TS_AT_VACCINE)) return ATTR_STRONG_PCT;
  if ((def == TS_AT_VACCINE && atk == TS_AT_VIRUS) || (def == TS_AT_VIRUS && atk == TS_AT_DATA) ||
      (def == TS_AT_DATA && atk == TS_AT_VACCINE)) return ATTR_WEAK_PCT;
  return 100;
}

// What an element does to this Digimon, from its resistance row.
static inline uint16_t elementEffVsDex(uint8_t element, int16_t dex) {
  if (element >= TS_ELEMENT_COUNT || !dexValid(dex)) return 100;
  return (uint16_t)TS_DEX_TBL[dex].resist[element] * 10;
}

// The whole matchup, as one percentage: the skill's element against the
// defender's row, times the attribute triangle of the two Digimon. Support skills
// never get here.
static inline uint16_t battleEffPct(int16_t atkDex, uint8_t element, int16_t defDex) {
  uint32_t e = elementEffVsDex(element, defDex);
  if (dexValid(atkDex) && dexValid(defDex))
    e = e * attrEffPct(TS_DEX_TBL[atkDex].attr, TS_DEX_TBL[defDex].attr) / 100;
  return (uint16_t)e;
}

// A form's own element: the element of its first attacking signature skill, which
// is what the form is known for (Agumon's Pepper Breath, Tentomon's Super Shocker).
// A form with no attacking signature (18 do, Botamon among them) falls back to
// the element it resists best, ties to the first.
//
// The card, the accent colour, the biome and the same-element bonus all ask this,
// so a form has one element wherever it is shown. Resistances alone could not
// carry it: 402 of the 469 forms tie for their best resistance, and only 272 of
// the 451 forms with an attacking signature resist that signature's element best.
static inline uint8_t homeElement(int16_t dex) {
  if (!dexValid(dex)) return TS_EL_NULL;
  const TsDexEntry &d = TS_DEX_TBL[dex];
  for (uint8_t i = 0; i < d.learnCount; i++) {
    const TsLearn &l = TS_LEARN_TBL[d.learnFirst + i];
    if (l.hours) break;                       // signatures come first, at hour 0
    const TsSkill &s = TS_SKILL_TBL[l.skill];
    if (s.cat != TS_SC_SUPPORT) return s.element;
  }
  uint8_t best = 0;
  for (uint8_t e = 1; e < TS_ELEMENT_COUNT; e++)
    if (d.resist[e] < d.resist[best]) best = e;
  return best;
}

// The same-element bonus: x1.5 when the skill's element is the attacker's own.
static inline bool hasStab(int16_t dex, uint8_t element) {
  return dexValid(dex) && element == homeElement(dex);
}

// Short names for the chips (ELECTRICITY does not fit a battle cell), and each
// element's colour. tools/gen_ladders.py reads ELEMENT_COL for the badges.
static const char *const ELEMENT_SHORT[TS_ELEMENT_COUNT] = {
  "FIRE", "WATER", "PLANT", "ICE", "ELEC", "EARTH", "STEEL", "WIND", "LIGHT", "DARK", "NULL",
};
static const uint16_t ELEMENT_COL[TS_ELEMENT_COUNT] = {
  0xEA87, 0x4C98, 0x3C49, 0x6DBA, 0xBCA1, 0xB447, 0x7BF1, 0x7C7A, 0xCD04, 0x6AD3, 0x8C4D,
};
static const uint8_t ELEMENT_COL_LIGHT[TS_ELEMENT_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 };

static inline const char *typeName(uint8_t el) {
  return el < TS_ELEMENT_COUNT ? ELEMENT_SHORT[el] : "?";
}
static inline const char *attrName(uint8_t a) {
  return a < TS_ATTR_COUNT ? TS_ATTR_NAME[a] : "?";
}
static inline uint16_t typeColor(uint8_t el) {
  return el < TS_ELEMENT_COUNT ? ELEMENT_COL[el] : 0x8410;
}
static inline bool typeColorIsLight(uint8_t el) {
  return el < TS_ELEMENT_COUNT && ELEMENT_COL_LIGHT[el];
}

// The UI accent of a Digimon: its element's colour.
static inline uint16_t dexAccent(int16_t dex) { return typeColor(homeElement(dex)); }

// The scene behind it: 0 meadow, 1 beach, 2 forest, 3 volcano, 4 mountain, 5 snow.
static inline uint8_t dexBiome(int16_t dex) {
  static const uint8_t BIOME[TS_ELEMENT_COUNT] = { 3, 1, 2, 5, 4, 4, 4, 4, 0, 2, 0 };
  return BIOME[homeElement(dex)];
}

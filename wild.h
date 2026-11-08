#pragma once
#include <stdint.h>
#include "roster.h"
#include "items.h"

// Wild encounters: the one way to meet a creature outside a boss.
//
// Everything here is a PURE FUNCTION of its arguments -- every roll is passed
// in rather than drawn from random(). That is not style: it is what lets
// wild_test pin both escape curves at their boundaries
// instead of asserting that some average came out about right. See CLAUDE.md
// § "A test that proves the transcription rather than the firmware".

// The variant roll uses one common denominator so 100 / 409600 is exactly 1/4096
// and each bonus point is exactly one percentage point.
constexpr uint32_t WILD_VARIANT_SCALE = 409600;
constexpr uint32_t WILD_VARIANT_BASE = 100;
constexpr uint32_t WILD_VARIANT_PER_POINT = 4096;
constexpr uint8_t WILD_VARIANT_BONUS_MAX = 15;

constexpr uint8_t WILD_BONUS_DROP_CHANCE = 30;   // percent, one extra item
// Which stage a wild Digimon is, against the stage your partner's level has reached:
// this much the same stage, this much one behind, and the rest (10%) one ahead.
constexpr uint8_t WILD_STAGE_SAME_PCT = 65;
constexpr uint8_t WILD_STAGE_BEHIND_PCT = 25;

uint32_t wildVariantThreshold(uint8_t bonus);
bool wildVariantForRoll(uint32_t roll, uint8_t bonus);

// A wild Digimon is met at your partner's level. Explore's Hard does not change
// the level or the stage: it plays the foe with the smarter AI and doubles the drops.
uint8_t wildLevelFor(uint8_t playerLevel);

// Normal grants one weighted item, hard two. Both then make one independent
// roll for a single bonus item.
uint8_t wildDropCount(bool hard, uint8_t bonusRoll);

// Your chance of getting away. 90% against something at or below your level,
// scaled down by the level ratio above it, with a 10% floor -- so fleeing a
// bad matchup is reliable but fleeing a much stronger one is not.
uint8_t wildEscapeChance(uint8_t playerLevel, uint8_t foeLevel);

// The foe's. It only starts considering escape at 40% HP (10%), rises linearly
// to 20% at 20% HP, then falls back to 10% at 10% and stays there -- a nearly
// beaten creature stops running and fights.
uint8_t wildFoeEscapeChance(uint16_t hp, uint16_t maxHp);

// The stage a Digimon of this level has reached on the stage clock.
uint8_t wildStageForLevel(uint8_t level);
// Which stage a wild Digimon is, from a 0..99 roll, for a partner of this level:
// 65% the stage that level has reached, 25% one behind, 10% one ahead. Mega+ is
// met only by a partner that IS Mega+ (`megaPlusPartner`, its form, not its level):
// for anyone else the level's stage stops at Mega, and a Mega's "ahead" stays Mega.
uint8_t wildStageFor(uint8_t level, bool megaPlusPartner, uint8_t roll);
// A Digimon whose wild TIER is that stage, and that can be drawn: the raisable
// forms, the wild-only ones and (until Jogress) the fusion-only ones, with Armor
// and Hybrid at their stat tier. 0 if the tier has none. The Aegiomon line is not
// in the roster at all.
int16_t wildPickSpecies(uint8_t stage, uint32_t roll);

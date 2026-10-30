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
constexpr uint8_t WILD_LEVEL_SPREAD = 5;         // either side of the player

uint32_t wildVariantThreshold(uint8_t bonus);
bool wildVariantForRoll(uint32_t roll, uint8_t bonus);

// The band a wild creature's level is drawn from. Hard opens the whole ladder;
// normal stays within WILD_LEVEL_SPREAD either side of the player so an
// encounter is always winnable.
uint8_t wildLevelMin(uint8_t playerLevel, bool hard);
uint8_t wildLevelMax(uint8_t playerLevel, bool hard);

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
// Which stage a wild Digimon of this level is, from a 0..99 roll: usually the
// stage its level reaches, a quarter of the time one behind.
uint8_t wildStageFor(uint8_t level, uint8_t roll);
// A Digimon of exactly that STAGE that has art. 0 if the stage has none. A stand-in
// until phase 2's wild rules (65/25/10 by stage, Armor and Hybrid by stat tier).
int16_t wildPickSpecies(uint8_t stage, uint32_t roll);

#pragma once
#include <Arduino.h>
#include "pet.h"

// Turn resolution. Deliberately free of any UI or hardware so the whole thing
// can be simulated headlessly -- see tools/emu.
//
// Integer maths only, like types.h: no floats anywhere on the MCU.

// Careful with the names: SI_SPD is magic defence, as Pet::spdStat() is, and
// SI_SPE is speed. TS_STAT_SPD and TS_SM_SPD are speed.
enum : uint8_t { SI_ATK = 0, SI_DEF, SI_SPA, SI_SPD, SI_SPE, SI_COUNT };

// Skill-driven stat changes stack in percent and last the fight. Three casts of a
// +30% Charge or a -20% Bind is the limit either way.
#define BTL_MOD_MIN -60
#define BTL_MOD_MAX 90
// A charged attack (Acceleration Boost, Spell Boost) hits twice as hard, once.
#define BTL_CHARGE_MULT 2

// A creature as it exists inside a battle. Built from the live pet or an
// opponent; nothing here is ever written back, which is what keeps ailments
// battle-only.
struct Combatant {
  int16_t dex = 0;
  uint8_t level = 1;
  uint16_t maxHp = 1, hp = 1;
  uint16_t base[SI_COUNT] = { 1, 1, 1, 1, 1 };  // before stat changes
  uint16_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  int8_t mod[SI_COUNT] = { 0, 0, 0, 0, 0 };     // percent, BTL_MOD_MIN..BTL_MOD_MAX
  uint8_t ailment = TS_AIL_NONE;  // never TS_AIL_CONFUSION: see confuseTurns
  uint8_t ailTurns = 0;      // sleep countdown
  uint8_t confuseTurns = 0;  // confusion runs alongside a real ailment
  bool recharge = false;     // TS_EF_RECHARGE spent this creature's next turn
  bool chargePhys = false;   // the next physical attack is boosted
  bool chargeMagic = false;  // the next magic attack is boosted
  bool variant = false;      // which sprite variant to stream
  char name[16] = "";

  bool fainted() const { return hp == 0; }
};

void combatantFromPet(Combatant &c, const Pet &p, uint8_t levelCap = 0);

// What one action did, so the UI can narrate it without recomputing anything.
struct TurnLog {
  uint16_t move = 0;
  uint16_t damage = 0;
  uint16_t effPct = 100;   // element and attribute matchup, percent
  uint8_t hits = 0;        // the skill's fixed hit count
  uint8_t inflicted = TS_AIL_NONE;
  int8_t modDelta = 0;     // for TS_EF_STAT, so the UI can say what moved
  uint8_t modMask = 0;     // TS_SM_*
  bool missed = false;
  bool crit = false;
  bool immune = false;     // a resistance of 0: it does nothing
  bool skipped = false;    // asleep, crystallized, fully paralysed or recharging
  bool hurtSelf = false;   // confusion
  bool healed = false;
  bool targetFainted = false;
};

uint16_t moddedStat(uint16_t base, int8_t pct);
// One hit. A skill's power is per hit; `hits` of them land.
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint16_t mv,
                      bool crit, uint8_t roll);

// True if `a` using `ma` acts before `b` using `mb`. A TS_EF_FIRST skill goes
// ahead, then the changed speed, and paralysis halves it.
bool battleMovesFirst(const Combatant &a, uint16_t ma,
                      const Combatant &b, uint16_t mb);

// One creature's action. Applies damage, statuses, stat changes and recoil.
// mv 0 is no action at all (an item used, a failed escape).
void battleAct(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log);

// Injury and poison chip damage, applied to one creature after both have acted.
void battleEndTurn(Combatant &c, TurnLog &log);

// Picks the attacker's skill. `smart` is what separates hard mode from easy:
// easy picks uniformly at random, hard reads the matchup, accuracy, whether a
// skill kills this turn, and whether a status or a stat boost is worth the turn
// it costs. Never 0: a Digimon with no attacking skill uses SKILL_BASIC, so two
// support-only Digimon still finish a fight.
uint16_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart);

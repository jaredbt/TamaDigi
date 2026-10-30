#include "battle.h"
#include "types.h"

// ---------- building a combatant ----------

void combatantFromPet(Combatant &c, const Pet &p, uint8_t levelCap) {
  c = Combatant();
  const int16_t dex = p.speciesId;
  const uint8_t lvl = levelCap && p.level() > levelCap ? levelCap : p.level();
  c.dex = dex;
  c.level = lvl;
  if (!dexValid(dex)) return;     // an egg: no stats, as it always had
  // ATK training also raises magic attack, DEF training also magic defence, the
  // same rule as Pet::spaStat() and Pet::spdStat().
  c.maxHp = petCalcHp(dex, lvl, p.deDigiCount);
  c.hp = c.maxHp;
  c.base[SI_ATK] = petCalcStat(dex, TS_STAT_ATK, lvl, p.trAtk);
  c.base[SI_DEF] = petCalcStat(dex, TS_STAT_DEF, lvl, p.trDef);
  c.base[SI_SPA] = petCalcStat(dex, TS_STAT_MATK, lvl, p.trAtk);
  c.base[SI_SPD] = petCalcStat(dex, TS_STAT_MDEF, lvl, p.trDef);
  c.base[SI_SPE] = petCalcStat(dex, TS_STAT_SPD, lvl, p.trSpe);
  for (int i = 0; i < MOVE_SLOTS; i++) c.moves[i] = p.moves[i];
  c.variant = p.variant;
  const char *nm = p.nick[0] ? p.nick : TS_DEX_TBL[dex].name;
  snprintf(c.name, sizeof(c.name), "%s", nm);
}

// ---------- stat changes ----------

uint16_t moddedStat(uint16_t base, int8_t pct) {
  uint32_t v = (uint32_t)base * (100 + pct) / 100;
  return v < 1 ? 1 : (v > 65535 ? 65535 : (uint16_t)v);
}

static uint16_t effStat(const Combatant &c, uint8_t idx) {
  uint16_t v = moddedStat(c.base[idx], c.mod[idx]);
  // Injury halves physical attack, paralysis halves speed -- the two ailments
  // that do something beyond chip damage or a lost turn.
  if (idx == SI_ATK && c.ailment == TS_AIL_INJURY) v = v / 2 ? v / 2 : 1;
  if (idx == SI_SPE && c.ailment == TS_AIL_PARALYSIS) v = v / 2 ? v / 2 : 1;
  return v;
}

// ---------- damage ----------

// roll is 217..255, the damage spread, passed in so tests can pin it.
uint16_t battleDamage(const Combatant &atk, const Combatant &def, uint16_t mv,
                      bool crit, uint8_t roll) {
  const TsSkill &m = skillEntry(mv);
  if (!mv || m.cat == TS_SC_SUPPORT) return 0;
  const bool phys = m.cat == TS_SC_PHYS;

  uint16_t A = phys ? effStat(atk, SI_ATK) : effStat(atk, SI_SPA);
  uint16_t D = phys ? effStat(def, SI_DEF) : effStat(def, SI_SPD);
  // A critical hit ignores the defender's boosts and the attacker's drops, so a
  // Guard Charge cannot make you immune to a lucky roll.
  if (crit) {
    A = phys ? atk.base[SI_ATK] : atk.base[SI_SPA];
    D = phys ? def.base[SI_DEF] : def.base[SI_SPD];
  }
  if (!D) D = 1;

  uint32_t dmg = (2UL * atk.level / 5 + 2) * skillPower(mv, atk.level) * A / D / 50 + 2;
  if (crit) dmg *= 2;
  if (phys ? atk.chargePhys : atk.chargeMagic) dmg *= BTL_CHARGE_MULT;
  if (hasStab(atk.dex, m.element)) dmg = dmg * SAME_ELEMENT_NUM / SAME_ELEMENT_DEN;
  uint16_t eff = battleEffPct(atk.dex, m.element, def.dex);
  dmg = dmg * eff / 100;
  if (eff == 0) return 0;               // immune: no chip, no minimum
  dmg = dmg * roll / 255;
  return dmg < 1 ? 1 : (dmg > 65535 ? 65535 : (uint16_t)dmg);
}

// ---------- turn order ----------

bool battleMovesFirst(const Combatant &a, uint16_t ma,
                      const Combatant &b, uint16_t mb) {
  const bool fa = ma && skillEntry(ma).effect == TS_EF_FIRST;
  const bool fb = mb && skillEntry(mb).effect == TS_EF_FIRST;
  if (fa != fb) return fa;
  uint16_t sa = effStat(a, SI_SPE), sb = effStat(b, SI_SPE);
  if (sa != sb) return sa > sb;
  return random(2) == 0;               // a genuine speed tie is a coin flip
}

// ---------- one action ----------

// TS_SM_* in SI_* order: the two tables number the stats differently.
static const uint8_t SM_OF_SI[SI_COUNT] = { TS_SM_ATK, TS_SM_DEF, TS_SM_MATK, TS_SM_MDEF, TS_SM_SPD };

static void applyMods(Combatant &c, uint8_t mask, int8_t delta) {
  for (int i = 0; i < SI_COUNT; i++) {
    if (!(mask & SM_OF_SI[i])) continue;
    int v = c.mod[i] + delta;
    c.mod[i] = v > BTL_MOD_MAX ? BTL_MOD_MAX : (v < BTL_MOD_MIN ? BTL_MOD_MIN : (int8_t)v);
  }
}

static void hurt(Combatant &c, uint16_t amount) {
  c.hp = (amount >= c.hp) ? 0 : c.hp - amount;
}

static void heal(Combatant &c, uint16_t amount) {
  uint32_t v = (uint32_t)c.hp + amount;
  c.hp = v > c.maxHp ? c.maxHp : (uint16_t)v;
}

// A status lands unless one is already there. Confusion is tracked separately so
// it can stack with a real status. No Digimon is immune by element.
static void inflict(Combatant &t, uint8_t ail, TurnLog &log) {
  if (ail == TS_AIL_CONFUSION) {
    if (!t.confuseTurns) {
      t.confuseTurns = 2 + random(3);
      log.inflicted = TS_AIL_CONFUSION;
    }
  } else if (t.ailment == TS_AIL_NONE) {
    t.ailment = ail;
    if (ail == TS_AIL_SLEEP) t.ailTurns = 2 + random(3);
    log.inflicted = ail;
  }
}

// Heal by percent of max HP, or by the skill's power and the user's magic defence:
// power x magic defence / 20 restores about half of max HP at power 30 across the
// roster (Time Stranger gives no formula), and training DEF feeds it.
static uint16_t healAmount(const Combatant &c, uint16_t mv) {
  const TsSkill &m = skillEntry(mv);
  if (m.param > 0) return (uint16_t)((uint32_t)c.maxHp * m.param / 100);
  return (uint16_t)((uint32_t)skillPower(mv, c.level) * effStat(c, SI_SPD) / 20);
}

// The one effect a skill carries, once it has connected. A damaging skill's effect
// only lands if the target is still standing, and an effect on the user always can.
static void applyEffect(Combatant &atk, Combatant &def, uint16_t mv, uint16_t dealt, TurnLog &log) {
  const TsSkill &m = skillEntry(mv);
  Combatant &t = (m.flags & TS_SKF_SELF) ? atk : def;
  const bool landed = !m.chance || (uint8_t)random(100) < m.chance;
  switch (m.effect) {
    case TS_EF_STATUS:
      if (landed && !t.fainted()) inflict(t, (uint8_t)m.param, log);
      break;
    case TS_EF_STAT:
      if (landed && !t.fainted()) {
        applyMods(t, m.statMask, m.param);
        log.modMask = m.statMask;
        log.modDelta = m.param;
      }
      break;
    case TS_EF_DRAIN:
      heal(atk, (uint16_t)((uint32_t)dealt * m.param / 100));
      log.healed = dealt != 0;
      break;
    case TS_EF_RECOIL:
      hurt(atk, (uint16_t)((uint32_t)atk.maxHp * m.param / 100));
      break;
    case TS_EF_HEAL:
      heal(atk, healAmount(atk, mv));
      log.healed = true;
      break;
    case TS_EF_RECHARGE:
      atk.recharge = true;
      break;
    case TS_EF_CHARGE_PHYS:
      atk.chargePhys = true;
      break;
    case TS_EF_CHARGE_MAGIC:
      atk.chargeMagic = true;
      break;
    case TS_EF_CURE:
      atk.ailment = TS_AIL_NONE;
      atk.ailTurns = 0;
      atk.confuseTurns = 0;
      break;
    case TS_EF_REMOVE_BUFFS:
      for (int i = 0; i < SI_COUNT; i++) if (def.mod[i] > 0) def.mod[i] = 0;
      break;
    case TS_EF_REMOVE_DEBUFFS:
      for (int i = 0; i < SI_COUNT; i++) if (atk.mod[i] < 0) atk.mod[i] = 0;
      break;
    default:
      break;
  }
}

void battleAct(Combatant &atk, Combatant &def, uint16_t mv, TurnLog &log) {
  log = TurnLog();
  log.move = mv;
  if (atk.fainted() || def.fainted()) { log.skipped = true; return; }

  // --- things that can cost the turn before a skill is even chosen
  if (atk.recharge) { atk.recharge = false; log.skipped = true; return; }
  if (atk.ailment == TS_AIL_CRYSTAL) {
    if (random(100) < 20) atk.ailment = TS_AIL_NONE;   // thaws
    else { log.skipped = true; return; }
  }
  if (atk.ailment == TS_AIL_SLEEP) {
    if (atk.ailTurns) atk.ailTurns--;
    if (atk.ailTurns == 0) atk.ailment = TS_AIL_NONE;
    else { log.skipped = true; return; }
  }
  if (atk.ailment == TS_AIL_PARALYSIS && random(100) < 25) { log.skipped = true; return; }
  if (atk.confuseTurns) {
    atk.confuseTurns--;
    if (random(100) < 33) {               // hits itself instead
      uint16_t self = (2UL * atk.level / 5 + 2) * 40 *
                          atk.base[SI_ATK] / (atk.base[SI_DEF] ? atk.base[SI_DEF] : 1) / 50 + 2;
      hurt(atk, self);
      log.hurtSelf = true;
      log.damage = self;
      return;
    }
  }

  if (!skillValid(mv) && mv != SKILL_BASIC) { log.skipped = true; return; }
  const TsSkill &m = skillEntry(mv);

  // --- accuracy
  if (m.effect != TS_EF_NEVER_MISS && (uint8_t)random(100) >= m.acc) {
    log.missed = true;
    return;
  }

  if (m.cat == TS_SC_SUPPORT) {
    applyEffect(atk, def, mv, 0, log);
    return;
  }

  // --- damage: a fixed number of hits, power per hit
  uint8_t hits = m.hits ? m.hits : 1;
  uint16_t total = 0;
  log.effPct = battleEffPct(atk.dex, m.element, def.dex);
  if (log.effPct == 0) { log.immune = true; return; }
  for (uint8_t h = 0; h < hits; h++) {
    bool crit = random(16) == 0;                 // ~6%, the base rate
    uint16_t d = battleDamage(atk, def, mv, crit, (uint8_t)(217 + random(39)));
    total = (uint32_t)total + d > 65535 ? 65535 : total + d;
    if (crit) log.crit = true;
    hurt(def, d);
    if (def.fainted()) { hits = h + 1; break; }
  }
  log.hits = hits;
  log.damage = total;
  // A boost is spent by the attack it powered, whichever kind that was.
  if (m.cat == TS_SC_PHYS) atk.chargePhys = false; else atk.chargeMagic = false;

  applyEffect(atk, def, mv, total, log);
  log.targetFainted = def.fainted();
}

// ---------- end of turn ----------

void battleEndTurn(Combatant &c, TurnLog &log) {
  log = TurnLog();
  if (c.fainted()) return;
  if (c.ailment == TS_AIL_INJURY || c.ailment == TS_AIL_POISON) {
    uint16_t chip = c.maxHp / 16;
    if (!chip) chip = 1;
    hurt(c, chip);
    log.damage = chip;
    log.inflicted = c.ailment;
    log.targetFainted = c.fainted();
  }
}

// ---------- skill choice ----------

uint16_t aiChooseMove(const Combatant &self, const Combatant &foe, bool smart) {
  uint16_t legal[MOVE_SLOTS + 1];
  uint8_t n = 0;
  bool canHit = false;
  for (int i = 0; i < MOVE_SLOTS; i++) {
    if (!skillValid(self.moves[i])) continue;
    legal[n++] = self.moves[i];
    if (TS_SKILL_TBL[self.moves[i]].cat != TS_SC_SUPPORT) canHit = true;
  }
  if (!canHit) legal[n++] = SKILL_BASIC;
  if (!smart) return legal[random(n)];

  int32_t bestScore = -2147483647;
  uint16_t best = legal[0];
  for (uint8_t i = 0; i < n; i++) {
    uint16_t mv = legal[i];
    const TsSkill &m = skillEntry(mv);
    int32_t sc;
    if (m.cat == TS_SC_SUPPORT) {
      // A support skill costs a whole turn, so it has to buy more than chip
      // damage would. Healing is worth it only when actually hurt; a boost is
      // worth it early and worthless once the stat is already stacked.
      if (m.effect == TS_EF_HEAL) {
        int missing = (int)self.maxHp - self.hp;
        sc = (missing * 100 / (self.maxHp ? self.maxHp : 1)) - 30;
      } else if (m.effect == TS_EF_STAT) {
        int stacked = 0, hit = 0;
        const Combatant &t = (m.flags & TS_SKF_SELF) ? self : foe;
        for (int k = 0; k < SI_COUNT; k++)
          if (m.statMask & SM_OF_SI[k]) { stacked += t.mod[k] * (m.param > 0 ? 1 : -1); hit++; }
        if (!hit) hit = 1;
        // diminishing: +30% is strong from nothing, pointless at the cap
        sc = 26 - (stacked * 30 / (hit * BTL_MOD_MAX));
        // and never set up when one more hit would finish you
        if (self.hp * 3 < self.maxHp) sc -= 40;
      } else if (m.effect == TS_EF_STATUS) {
        sc = foe.ailment == TS_AIL_NONE ? m.chance / 3 : 0;
      } else if (m.effect == TS_EF_CURE) {
        sc = (self.ailment != TS_AIL_NONE || self.confuseTurns) ? 40 : -10;
      } else {
        sc = 5;
      }
    } else {
      uint32_t dmg = (uint32_t)battleDamage(self, foe, mv, false, 236) * (m.hits ? m.hits : 1);
      if (dmg > 30000) dmg = 30000;
      sc = (int32_t)dmg;
      if (dmg >= foe.hp) sc += 1000;              // a kill this turn beats all
      sc = sc * m.acc / 100;                      // discount what tends to miss
      if (m.effect == TS_EF_RECHARGE) sc -= (int32_t)dmg / 4;   // a free turn for the foe
      if (m.effect == TS_EF_RECOIL) sc -= (int32_t)dmg / 6;
      if (m.effect == TS_EF_STATUS && foe.ailment == TS_AIL_NONE) sc += m.chance / 4;
    }
    if (sc > bestScore) { bestScore = sc; best = mv; }
  }
  return best;
}

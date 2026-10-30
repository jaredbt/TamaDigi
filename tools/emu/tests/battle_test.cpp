// Exercises the real battle engine: damage, Time Stranger's element multipliers, the
// same-element bonus, stat changes, the six statuses, every skill effect, turn
// order, and a full fight to a KO.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "roster.h"
#include "types.h"
#include <cstdio>

#ifndef TAMA_NATIVE_TEST
uint32_t g_seed = 12345;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
#endif

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

// Fewer on the emulated S3, where each suite has a time budget. The measurement is the
// same; only the sample is smaller.
#ifdef TAMA_NATIVE_TEST
#define FIGHT_REPS 1
#else
#define FIGHT_REPS 8
#endif

static const int16_t AGUMON = 21, GREYMON = 89, GARURUMON = 86, ANGEMON = 83, DEVIMON = 155, OGREMON = 140,
                     BIRDRAMON = 98;

static void mk(Combatant &c, int16_t dex, uint8_t lvl) {
  Pet p;
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.relearnFromLevel();
  combatantFromPet(c, p);
}

static uint16_t findMove(const char *name) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++)
    if (!strcmp(TS_SKILL_TBL[i].name, name)) return i;
  return 0;
}

// The first signature attack with this effect, read from the table so the test does
// not depend on which skill happens to carry it.
static uint16_t attackWith(uint8_t effect) {
  for (uint16_t i = TS_SKILL_FIRST_SIGNATURE; i < MOVE_COUNT; i++)
    if (TS_SKILL_TBL[i].effect == effect && TS_SKILL_TBL[i].cat != TS_SC_SUPPORT) return i;
  return 0;
}

// Runs one action until it does not miss, so a test does not depend on the roll.
static void connect(Combatant &a, Combatant &d, uint16_t mv, TurnLog &lg) {
  for (int i = 0; i < 200; i++) {
    battleAct(a, d, mv, lg);
    if (!lg.missed) return;
  }
}

int main() {
  TurnLog lg;

  // --- stat changes are Time Stranger's percentages
  ck(moddedStat(100, 0) == 100, "no change is 1x");
  ck(moddedStat(100, 30) == 130, "+30% is 1.3x");
  ck(moddedStat(100, -20) == 80, "-20% is 0.8x");

  // --- the skill's element against the defender's own row
  ck(elementEffVsDex(TS_EL_FIRE, GREYMON) == TS_DEX_TBL[GREYMON].resist[TS_EL_FIRE] * 10,
     "an element's multiplier is the defender's resistance row");
  {
    Combatant a, weak, strong;
    mk(a, AGUMON, 50); mk(weak, AGUMON, 50); mk(strong, AGUMON, 50);
    const uint16_t ice = findMove("ICICLE EDGE");     // AGUMON takes 1.5x from ice
    const uint16_t fire = findMove("WOLKENAPALM");    // and 0.5x from fire
    ck(TS_DEX_TBL[AGUMON].resist[TS_EL_ICE] == 15 && TS_DEX_TBL[AGUMON].resist[TS_EL_FIRE] == 5,
       "AGUMON's row has both ends to test");
    a.mod[SI_ATK] = 0;
    uint32_t iceHit = battleDamage(a, weak, ice, false, 255);
    uint32_t fireHit = battleDamage(a, strong, fire, false, 255);
    printf("     ICICLE EDGE per hit %u, WOLKENAPALM %u\n", (unsigned)iceHit, (unsigned)fireHit);
    ck(battleEffPct(AGUMON, TS_EL_ICE, AGUMON) == 150, "AGUMON into its weak element is 150%");
    ck(battleEffPct(AGUMON, TS_EL_FIRE, AGUMON) == 50, "and into its strong one, 50%");
  }

  // --- a resistance of 0 is a real immunity, and says so
  {
    int16_t ghost = 0; uint8_t el = 0;
    for (int16_t d = 1; d <= DEX_COUNT && !ghost; d++)
      for (uint8_t e = 0; e < TS_ELEMENT_COUNT; e++)
        if (TS_DEX_TBL[d].resist[e] == 0) { ghost = d; el = e; break; }
    uint16_t mv = 0;
    for (uint16_t m = 1; m <= 22 && !mv; m++) if (TS_SKILL_TBL[m].element == el) mv = m;
    Combatant a, d;
    mk(a, GREYMON, 50); mk(d, ghost, 50);
    ck(battleDamage(a, d, mv, false, 255) == 0, "0 damage from the element a Digimon is immune to");
    const uint16_t hp = d.hp;
    connect(a, d, mv, lg);
    ck(lg.immune && d.hp == hp, "and the log says so");
  }

  // --- the attribute triangle sits on top: Vaccine > Virus > Data > Vaccine
  ck(attrEffPct(TS_AT_VACCINE, TS_AT_VIRUS) == ATTR_STRONG_PCT, "a Vaccine hits a Virus harder");
  ck(attrEffPct(TS_AT_VACCINE, TS_AT_DATA) == ATTR_WEAK_PCT, "and a Data softer");
  ck(attrEffPct(TS_AT_VIRUS, TS_AT_DATA) == ATTR_STRONG_PCT, "a Virus hits a Data harder");
  ck(attrEffPct(TS_AT_DATA, TS_AT_VACCINE) == ATTR_STRONG_PCT, "a Data hits a Vaccine harder");
  ck(attrEffPct(TS_AT_FREE, TS_AT_VIRUS) == 100 && attrEffPct(TS_AT_NODATA, TS_AT_DATA) == 100 &&
     attrEffPct(TS_AT_VARIABLE, TS_AT_VACCINE) == 100 && attrEffPct(TS_AT_DATA, TS_AT_UNKNOWN) == 100,
     "the rest are neutral");
  {
    int16_t virus = 0;
    for (int16_t d = 1; d <= DEX_COUNT && !virus; d++) if (TS_DEX_TBL[d].attr == TS_AT_VIRUS) virus = d;
    ck(battleEffPct(ANGEMON, TS_EL_NULL, virus) ==
       elementEffVsDex(TS_EL_NULL, virus) * attrEffPct(TS_DEX_TBL[ANGEMON].attr, TS_AT_VIRUS) / 100,
       "and the two multiply");
  }

  // --- a form's own element, and the same-element bonus
  ck(homeElement(AGUMON) == TS_EL_FIRE, "AGUMON's element is its Pepper Breath's: Fire");
  ck(homeElement(DEVIMON) == TS_EL_DARK, "DEVIMON's is Dark, though it resists Fire as well");
  {
    int16_t tentomon = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (!strcmp(TS_DEX_TBL[d].name, "TENTOMON")) tentomon = d;
    ck(homeElement(tentomon) == TS_EL_ELECTRICITY, "TENTOMON's is Electricity, which it does not resist");
    ck(TS_LEARN_TBL[TS_DEX_TBL[6].learnFirst].skill && TS_SKILL_TBL[TS_LEARN_TBL[TS_DEX_TBL[6].learnFirst].skill].cat == TS_SC_SUPPORT,
       "BOTAMON has no attacking signature");
    ck(TS_DEX_TBL[6].resist[homeElement(6)] <= 5, "so its element is the one it resists best");
    Combatant a, d;
    mk(a, AGUMON, 50); mk(d, GARURUMON, 50);
    const uint16_t fire = findMove("WOLKENAPALM"), wind = findMove("WIND CLAW");
    ck(skillPower(fire, 50) == skillPower(wind, 50), "two skills of equal power");
    const uint32_t f = battleDamage(a, d, fire, false, 255) * 1000UL / battleEffPct(AGUMON, TS_EL_FIRE, GARURUMON);
    const uint32_t w = battleDamage(a, d, wind, false, 255) * 1000UL / battleEffPct(AGUMON, TS_EL_WIND, GARURUMON);
    printf("     matchup-neutral damage: own element %u, other %u\n", (unsigned)f, (unsigned)w);
    ck(f * 10 >= w * 14 && f * 10 <= w * 16, "the own-element skill hits about 1.5x as hard");
  }

  // --- learnable skills' power comes from the level band
  {
    const uint16_t m = findMove("WOLKENAPALM");
    ck(skillPower(m, 1) == 40 && skillPower(m, 24) == 40, "tier I below level 25");
    ck(skillPower(m, 25) == 60 && skillPower(m, 60) == 60, "tier II at 25-60");
    ck(skillPower(m, 61) == 75 && skillPower(m, 100) == 75, "tier III from 61");
    const uint16_t sig = TS_SKILL_FIRST_SIGNATURE;
    ck(skillPower(sig, 1) == skillPower(sig, 99), "a signature has one power at every level");
  }

  // --- a fixed hit count: every hit of it lands, power per hit
  {
    const uint16_t m = findMove("CRESCENT LEAF");   // 7 hits
    ck(TS_SKILL_TBL[m].hits == 7, "CRESCENT LEAF is seven hits");
    bool always = true;
    for (int i = 0; i < 40; i++) {
      Combatant a, d;
      mk(a, GREYMON, 80); mk(d, GARURUMON, 99);
      d.base[SI_DEF] = 2000;
      connect(a, d, m, lg);
      if (lg.hits != 7) always = false;
    }
    ck(always, "and it is always seven, not a roll");
  }

  // --- Attack Charge raises the user, Attack Bind lowers the foe
  {
    Combatant a, d;
    mk(a, OGREMON, 50); mk(d, OGREMON, 50);
    const uint16_t hit = findMove("HEAVY STRIKE");
    const uint16_t before = battleDamage(a, d, hit, false, 255);
    connect(a, d, findMove("ATTACK CHARGE"), lg);
    const uint16_t after = battleDamage(a, d, hit, false, 255);
    printf("     HEAVY STRIKE before %u, after ATTACK CHARGE %u (+%d%%)\n", before, after, a.mod[SI_ATK]);
    ck(a.mod[SI_ATK] == 30 && d.mod[SI_ATK] == 0, "ATTACK CHARGE is +30% on the user alone");
    ck(lg.modMask == TS_SM_ATK && lg.modDelta == 30, "and the log says what moved");
    ck(after > before * 12 / 10, "and raises damage");
    connect(a, d, findMove("ATTACK BIND"), lg);
    ck(d.mod[SI_ATK] == -20 && a.mod[SI_ATK] == 30, "ATTACK BIND is -20% on the foe alone");
    for (int i = 0; i < 8; i++) connect(a, a, findMove("ATTACK CHARGE"), lg);
    ck(a.mod[SI_ATK] == BTL_MOD_MAX, "stat changes stop at their ceiling");
    // the statMask bits and SI_* run in different orders
    connect(a, d, findMove("SPEED CHARGE"), lg);
    ck(a.mod[SI_SPE] == 30 && a.mod[SI_SPD] == 0, "SPEED CHARGE moves speed, not magic defence");
    connect(a, d, findMove("MENTAL CHARGE"), lg);
    ck(a.mod[SI_SPD] == 30, "and MENTAL CHARGE moves magic defence");
    connect(a, d, findMove("RESET BODY"), lg);
    connect(d, a, findMove("RESET BODY"), lg);
    ck(a.mod[SI_ATK] == 0 && a.mod[SI_SPE] == 0, "RESET BODY clears the target's boosts");
    connect(a, d, findMove("ATTACK BIND"), lg);
    connect(d, a, findMove("ATTACK BIND"), lg);
    connect(a, d, findMove("DISPEL"), lg);
    ck(a.mod[SI_ATK] == 0 && d.mod[SI_ATK] < 0, "DISPEL clears the user's own drops only");
  }

  // --- turn order follows speed, and a moves-first skill beats it
  {
    Combatant fast, slow;
    mk(fast, GREYMON, 50); mk(slow, GREYMON, 50);
    fast.base[SI_SPE] = 100; slow.base[SI_SPE] = 50;
    const uint16_t plain = findMove("HEAVY STRIKE"), first = attackWith(TS_EF_FIRST);
    ck(first != 0, "a moves-first skill exists");
    ck(battleMovesFirst(fast, plain, slow, plain), "the faster creature acts first");
    ck(battleMovesFirst(slow, first, fast, plain), "a moves-first skill beats raw speed");
    slow.base[SI_SPE] = 80;
    fast.ailment = TS_AIL_PARALYSIS;
    ck(!battleMovesFirst(fast, plain, slow, plain), "paralysis halves speed");
  }

  // --- the six statuses, and no Digimon is immune to one by its element
  {
    // Ice-element GARURUMON crystallized: the old engine's element immunities are gone.
    const uint16_t net = findMove("TEXTURE BLOW");
    bool crystal = false;
    for (int i = 0; i < 80 && !crystal; i++) {
      Combatant a, d;
      mk(a, GARURUMON, 50); mk(d, GARURUMON, 50);
      connect(a, d, net, lg);
      crystal = d.ailment == TS_AIL_CRYSTAL;
    }
    ck(crystal, "an Ice-element form can still be crystallized");
    uint16_t inj = 0;
    for (uint16_t i = TS_SKILL_FIRST_SIGNATURE; i < MOVE_COUNT && !inj; i++)
      if (TS_SKILL_TBL[i].effect == TS_EF_STATUS && TS_SKILL_TBL[i].param == TS_AIL_INJURY &&
          TS_SKILL_TBL[i].element == TS_EL_FIRE) inj = i;
    ck(inj != 0, "a Fire skill inflicts Injury");
    bool injured = false;
    for (int i = 0; i < 300 && !injured; i++) {
      Combatant a, d;
      mk(a, GREYMON, 50); mk(d, GREYMON, 50);
      d.maxHp = d.hp = 60000;
      connect(a, d, inj, lg);
      injured = d.ailment == TS_AIL_INJURY;
    }
    ck(injured, "and a Fire Digimon can be injured by it");

    Combatant a, d;
    mk(a, OGREMON, 50); mk(d, OGREMON, 50);
    const uint16_t hit = findMove("HEAVY STRIKE"), magic = findMove("POWER ENERGY");
    const uint16_t clean = battleDamage(a, d, hit, false, 255);
    const uint16_t cleanM = battleDamage(a, d, magic, false, 255);
    a.ailment = TS_AIL_INJURY;
    ck(battleDamage(a, d, hit, false, 255) < clean * 6 / 10, "Injury halves physical damage");
    ck(battleDamage(a, d, magic, false, 255) == cleanM, "and leaves magic alone");
    const uint16_t hp = a.hp;
    battleEndTurn(a, lg);
    ck(a.hp == hp - a.maxHp / 16, "Injury chips a sixteenth of max HP at end of turn");
    a.ailment = TS_AIL_POISON;
    const uint16_t hp2 = a.hp;
    battleEndTurn(a, lg);
    ck(a.hp == hp2 - a.maxHp / 16, "and so does Poison");

    Combatant c, e;
    mk(c, GREYMON, 50); mk(e, GREYMON, 50);
    c.ailment = TS_AIL_CRYSTAL;
    int skipped = 0, acted = 0;
    for (int i = 0; i < 200; i++) {
      c.ailment = TS_AIL_CRYSTAL;
      battleAct(c, e, hit, lg);
      e.hp = e.maxHp;
      if (lg.skipped) skipped++; else acted++;
    }
    ck(skipped > 120 && acted > 10, "Crystallization costs turns until it wears off, about one in five");
    ck(TS_AIL_NONE == 0 && TS_AIL_CONFUSION == 6, "the statuses run 1-6");
  }

  // --- every effect a skill can carry
  {
    Combatant a, d;
    const uint16_t drain = attackWith(TS_EF_DRAIN);
    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    a.hp = 10;
    connect(a, d, drain, lg);
    ck(a.hp > 10 && lg.healed, "drain heals the user from the damage dealt");

    const uint16_t recoil = attackWith(TS_EF_RECOIL);
    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    d.maxHp = d.hp = 60000;
    connect(a, d, recoil, lg);
    ck(a.hp == a.maxHp - a.maxHp * TS_SKILL_TBL[recoil].param / 100 || a.fainted(),
       "recoil costs a share of the user's max HP");

    const uint16_t heal = findMove("HEAL");
    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    a.hp = 1;
    connect(a, d, heal, lg);
    printf("     HEAL restored %u of %u\n", a.hp - 1, a.maxHp);
    ck(a.hp > a.maxHp / 3 && a.hp < a.maxHp, "HEAL, power and magic defence: a real part of max HP");

    const uint16_t recharge = attackWith(TS_EF_RECHARGE);
    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    d.maxHp = d.hp = 60000;
    connect(a, d, recharge, lg);
    ck(a.recharge, "a recharge skill spends the next turn");
    battleAct(a, d, recharge, lg);
    ck(lg.skipped && !a.recharge, "and only that one");

    const uint16_t boost = findMove("ACCELERATION BOOST"), hit = findMove("HEAVY STRIKE");
    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    const uint16_t plain = battleDamage(a, d, hit, false, 255);
    connect(a, d, boost, lg);
    ck(a.chargePhys && !a.chargeMagic, "ACCELERATION BOOST charges the next physical attack");
    const uint16_t charged = battleDamage(a, d, hit, false, 255);
    ck(charged >= plain * 2 - 2, "which hits twice as hard");
    connect(a, d, hit, lg);
    ck(!a.chargePhys, "and is spent by it");
    connect(a, d, findMove("SPELL BOOST"), lg);
    ck(a.chargeMagic && !a.chargePhys, "SPELL BOOST is the magic twin");

    mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
    a.ailment = TS_AIL_POISON; a.confuseTurns = 3;
    for (int i = 0; i < 40 && a.ailment; i++) connect(a, d, findMove("RESTORE"), lg);
    ck(a.ailment == TS_AIL_NONE && a.confuseTurns == 0, "RESTORE clears the statuses, confusion too");

    const uint16_t never = attackWith(TS_EF_NEVER_MISS);
    ck(never != 0, "a never-miss skill exists");
    bool missed = false;
    for (int i = 0; i < 200; i++) {
      mk(a, GREYMON, 60); mk(d, GARURUMON, 60);
      battleAct(a, d, never, lg);
      if (lg.missed) missed = true;
    }
    ck(!missed, "and it never misses");
  }

  // --- the basic attack is in no slot and always works
  {
    Combatant a, d;
    mk(a, GREYMON, 50); mk(d, GARURUMON, 50);
    for (int i = 0; i < MOVE_SLOTS; i++) a.moves[i] = 0;
    const uint16_t hp = d.hp;
    connect(a, d, SKILL_BASIC, lg);
    ck(d.hp < hp, "the basic attack does damage from no slot at all");
    ck(lg.move == SKILL_BASIC, "and is logged as itself");
    d.hp = hp;
    battleAct(a, d, 0, lg);
    ck(lg.skipped && d.hp == hp, "while 0 stays no action: an item, a failed escape");
  }

  // --- two Digimon with nothing to hit with still finish a fight
  {
    Combatant A, B;
    mk(A, 5, 1); mk(B, 6, 1);       // PUNIMON and BOTAMON, freshly hatched
    int t = 0;
    while (!A.fainted() && !B.fainted() && t < 300) {
      t++;
      uint16_t ma = aiChooseMove(A, B, false), mb = aiChooseMove(B, A, false);
      Combatant *f = &A, *s = &B; uint16_t mf = ma, ms = mb;
      if (!battleMovesFirst(A, ma, B, mb)) { f = &B; s = &A; mf = mb; ms = ma; }
      battleAct(*f, *s, mf, lg);
      if (!s->fainted()) battleAct(*s, *f, ms, lg);
      battleEndTurn(A, lg); battleEndTurn(B, lg);
    }
    printf("     two newborns: fight over on turn %d\n", t);
    ck(A.fainted() || B.fainted(), "two newborns with support skills still finish a fight");
  }

  // --- how long does a fight actually last, at each stage of a life?
  {
    const int16_t roster[] = { GREYMON, GARURUMON, ANGEMON, DEVIMON, OGREMON, BIRDRAMON };
    for (uint8_t lvl : { 10, 30, 50, 70, 99 }) {
      int total = 0, fights = 0, shortest = 999, longest = 0;
      for (int i = 0; i < 6; i++)
        for (int j = 0; j < 6; j++) {
          if (i == j) continue;
          for (int rep = 0; rep < FIGHT_REPS; rep++) {
            Combatant X, Y;
            mk(X, roster[i], lvl); mk(Y, roster[j], lvl);
            int t = 0;
            while (!X.fainted() && !Y.fainted() && t < 200) {
              t++;
              uint16_t mx = aiChooseMove(X, Y, false), my = aiChooseMove(Y, X, false);
              Combatant *f = &X, *sd = &Y; uint16_t mf = mx, ms = my;
              if (!battleMovesFirst(X, mx, Y, my)) { f = &Y; sd = &X; mf = my; ms = mx; }
              battleAct(*f, *sd, mf, lg);
              if (!sd->fainted()) battleAct(*sd, *f, ms, lg);
              battleEndTurn(X, lg); battleEndTurn(Y, lg);
            }
            total += t; fights++;
            if (t < shortest) shortest = t;
            if (t > longest) longest = t;
          }
        }
      printf("     %d fights at L%u: average %.1f turns (shortest %d, longest %d)\n",
             fights, lvl, (double)total / fights, shortest, longest);
      ck(longest < 200, "a fight reaches a conclusion");
    }
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}

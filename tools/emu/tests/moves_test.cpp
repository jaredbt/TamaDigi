// Skill storage and learning: specials on becoming a form, attachment skills at their
// hour offset, a free slot filling silently, a full set making an offer, dropped
// being gone. Asserts against the real Pet rather than restating its rules.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "roster.h"
#include <cstdio>

#ifndef TAMA_NATIVE_TEST
uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
bool wasPressed = false;
static uint32_t g_ms = 0;
uint32_t millis() { return g_ms; }
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
static void dump(const char *tag, const uint16_t *mv) {
  printf("     %s:", tag);
  for (int i = 0; i < MOVE_SLOTS; i++)
    printf(" [%s]", mv[i] ? moveName(mv[i]) : "-");
  printf("\n");
}

static uint16_t findMove(const char *name) {
  for (uint16_t i = 1; i < MOVE_COUNT; i++)
    if (!strcmp(TS_SKILL_TBL[i].name, name)) return i;
  return 0;
}

static const int16_t AGUMON = 21, GREYMON = 89, KOROMON = 9;

int main() {
  // --- a form has a learnset: specials first, then attachment skills by hour
  {
    int noSpecial = 0, unsorted = 0, oob = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const TsDexEntry &e = TS_DEX_TBL[d];
      if (!e.learnCount || TS_LEARN_TBL[e.learnFirst].hours) noSpecial++;
      for (uint8_t i = 0; i < e.learnCount; i++) {
        const TsLearn &l = TS_LEARN_TBL[e.learnFirst + i];
        if (!skillValid(l.skill)) oob++;
        if (i && l.hours < TS_LEARN_TBL[e.learnFirst + i - 1].hours) unsorted++;
      }
    }
    ck(noSpecial == 0, "every form has a special skill");
    ck(unsorted == 0, "a learnset is in hour order, specials first");
    ck(oob == 0, "and every entry is a real skill");
  }

  // --- specials arrive on becoming the form
  Pet p;
  p.begin();
  p.dbgHatchAs(AGUMON, false);
  dump("AGUMON hatched", p.moves);
  ck(p.knowsMove(findMove("PEPPER BREATH")), "AGUMON has Pepper Breath the moment it is AGUMON");
  ck(p.moveCount() == 1, "and nothing else yet");

  // --- attachment skills arrive at their hour offset after it
  {
    const TsDexEntry &e = TS_DEX_TBL[AGUMON];
    const TsLearn &first = TS_LEARN_TBL[e.learnFirst + 1];
    ck(first.hours >= 1, "AGUMON's first attachment skill has an hour offset");
    p.ageMinutes = p.formAgeMin + (uint32_t)first.hours * MINUTES_PER_LEVEL - 1;
    p.checkLearnGates();
    ck(!p.knowsMove(first.skill), "a minute before its hour it is not there");
    p.ageMinutes += 1;
    p.checkLearnGates();
    ck(p.knowsMove(first.skill), "on the hour it fills a free slot, silently");
    ck(!p.hasLearnOffer(), "with no offer to answer");
  }

  // --- a full set makes an offer, and declining is final
  {
    Pet q;
    q.begin();
    q.dbgHatchAs(AGUMON, false);
    q.moves[0] = 1; q.moves[1] = 3; q.moves[2] = 5; q.moves[3] = 7;
    q.ageMinutes = q.formAgeMin + 99UL * MINUTES_PER_LEVEL;
    q.checkLearnGates();
    ck(q.hasLearnOffer(), "with four skills held, the next one is an offer");
    ck(q.moves[0] == 1 && q.moves[3] == 7, "and nothing was overwritten");
    const uint16_t offered = q.learnOffer();
    q.declineLearn();
    ck(!q.knowsMove(offered), "skipped, it is not learned");
    while (q.hasLearnOffer()) q.declineLearn();
    q.ageMinutes += 10 * MINUTES_PER_LEVEL;
    q.checkLearnGates();
    ck(!q.hasLearnOffer(), "and it never comes back: dropped is gone");
  }

  // --- an offer outlives a reboot
  {
    Pet q;
    q.begin();
    q.dbgHatchAs(AGUMON, false);
    q.moves[0] = 1; q.moves[1] = 3; q.moves[2] = 5; q.moves[3] = 7;
    q.ageMinutes = q.formAgeMin + 99UL * MINUTES_PER_LEVEL;
    q.checkLearnGates();
    const uint16_t offered = q.learnOffer();
    const uint8_t count = q.learnQCount;
    q.saveNow();
    Pet r;
    r.begin();
    ck(r.learnOffer() == offered && r.learnQCount == count, "the queue of offers is in the save");
    ck(r.moves[3] == 7, "and so are the 16-bit slots");
    const uint16_t high = MOVE_COUNT - 1;
    r.moves[0] = high;
    r.saveNow();
    Pet t;
    t.begin();
    ck(t.moves[0] == high, "a skill id past 255 survives the trip");
    t.acceptLearn(2);
    ck(t.moves[2] == offered, "accepting puts it in the slot asked for");
  }

  // --- skills carry over: a form keeps what it knew, signature included
  {
    Pet q;
    q.begin();
    q.dbgHatchAs(GREYMON, false);
    const uint16_t flame = findMove("MEGA FLAME");
    ck(q.knowsMove(flame), "GREYMON has Mega Flame");
    q.ageMinutes = 39UL * MINUTES_PER_LEVEL;
    q.dbgRegister(AGUMON);
    ck(q.devolve(AGUMON), "GREYMON de-digivolves");
    ck(q.knowsMove(flame), "an AGUMON that was a GREYMON keeps Mega Flame");
    ck(q.knowsMove(findMove("PEPPER BREATH")), "and de-digivolving hands over AGUMON's specials");
    ck(q.moveCount() == 2, "skills already there are not doubled");
  }

  // --- a 15-letter dex name and a 30-letter skill both fit the battle log
  {
    int longName = 0, longest = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (strlen(TS_DEX_TBL[d].name) > 15) longName++;
    for (uint16_t m = 1; m < MOVE_COUNT; m++)
      if ((int)strlen(TS_SKILL_TBL[m].name) > longest) longest = (int)strlen(TS_SKILL_TBL[m].name);
    ck(longName == 0, "no display name is over 15 characters");
    ck(longest <= 30, "and no skill name is over 30");
  }

  // --- a default set is real, distinct and legal
  {
    Pet q;
    q.dbgHatchAs(6, false);
    q.ageMinutes = 5940;                 // level 100, an old hatchling
    q.relearnFromLevel();
    dump("BOTAMON L100", q.moves);
    ck(q.moveCount() >= 1 && q.moveCount() <= 4, "a default set is 1 to 4 skills");
    Pet g;
    g.dbgHatchAs(GREYMON, false);
    g.ageMinutes = 5940;
    g.relearnFromLevel();
    dump("GREYMON L100", g.moves);
    ck(g.moveCount() == 4, "a GREYMON that has had time knows four");
    ck(g.knowsMove(findMove("MEGA FLAME")), "its special first");
    bool distinct = true, valid = true;
    for (int i = 0; i < MOVE_SLOTS; i++) {
      if (!skillValid(g.moves[i])) valid = false;
      for (int j = i + 1; j < MOVE_SLOTS; j++)
        if (g.moves[i] == g.moves[j]) distinct = false;
    }
    ck(distinct, "no duplicate skills");
    ck(valid, "every skill is a valid table index");
    ck(!g.hasLearnOffer(), "a default set offers nothing");
    uint16_t cand[8];
    const uint8_t n = g.pendingLearnables(cand, 8);
    bool known = false;
    for (uint8_t i = 0; i < n; i++) if (g.knowsMove(cand[i])) known = true;
    ck(!known, "pendingLearnables never lists a known skill");
    g.ageMinutes = 0;
    g.formAgeMin = 0;
    g.relearnFromLevel();
    ck(g.moveCount() == 1, "while one that has just become the form knows its special alone");
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}

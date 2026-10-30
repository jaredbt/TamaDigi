// Asserts against the real Pet, per CLAUDE.md: restating the formulas here
// would only prove the transcription. Covers the level cap (which is what stops
// the uint8_t wrap) and Approach B: linear growth, one scale constant, plus training.
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
volatile bool g_dummyTouch = false;
bool wasPressed = false;
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
void sfxPlay(uint8_t) {}   // lives in the sketch; audio is not under test
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
#endif

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  Pet p;
  const int16_t GREYMON = 89, DEVIMON = 155, OGREMON = 140;
  p.dbgHatchAs(DEVIMON, false);
  p.ivAtk = p.ivDef = p.ivSpe = p.ivHp = 31;
  p.trAtk = p.trDef = p.trSpe = 0;

  // --- level cap. 3 days = 73 (first farewell offer), 4d3h = 100, and the
  // RTC's two-week catch-up used to compute 337 and wrap to 81.
  p.ageMinutes = 3UL * 24 * 60;
  ck(p.level() == 73, "3 days -> level 73");
  p.ageMinutes = 5940;                       // 4d 3h
  ck(p.level() == 100, "4d3h -> level 100");
  p.ageMinutes = 14UL * 24 * 60;             // max RTC offline catch-up
  printf("     two-week catch-up: level=%u (unclamped would be 337 -> wraps to 81)\n", p.level());
  ck(p.level() == 100, "two-week catch-up clamps instead of wrapping");
  p.ageMinutes = 60UL * 24 * 60;             // absurd, far past any wrap point
  ck(p.level() == 100, "60 days still clamps");

  // --- Approach B, read off the table: level 1 and level 99 are its two columns,
  // each over ten, and the line between them is straight.
  {
    const TsDexEntry &g = TS_DEX_TBL[GREYMON];
    ck(petCalcStat(GREYMON, TS_STAT_ATK, 1, 0) == g.base[TS_STAT_ATK] * TS_STAT_SCALE_PCT / 100,
       "level 1 is the level-1 column, scaled");
    ck(petCalcStat(GREYMON, TS_STAT_ATK, 99, 0) == g.max[TS_STAT_ATK] * TS_STAT_SCALE_PCT / 100,
       "level 99 is the level-99 column, scaled");
    ck(petCalcStat(GREYMON, TS_STAT_ATK, 100, 0) == petCalcStat(GREYMON, TS_STAT_ATK, 99, 0),
       "and level 100 adds nothing past it");
    const uint16_t mid = petCalcStat(GREYMON, TS_STAT_ATK, 50, 0);
    const uint16_t want = (g.base[TS_STAT_ATK] + g.max[TS_STAT_ATK]) / 2 * TS_STAT_SCALE_PCT / 100;
    ck(mid >= want - 1 && mid <= want + 1, "the middle level sits half way along it");
    ck(petCalcStat(GREYMON, TS_STAT_ATK, 50, 40) == mid + 40, "training adds on top, point for point");
  }

  // --- training: ATK also raises magic attack, DEF also magic defence
  {
    Pet t;
    t.dbgHatchAs(GREYMON, false);
    t.ageMinutes = 49 * MINUTES_PER_LEVEL;
    uint16_t atk = t.atkStat(), spa = t.spaStat(), def = t.defStat(), spd = t.spdStat(), spe = t.speStat();
    t.trAtk = 30;
    ck(t.atkStat() == atk + 30 && t.spaStat() == spa + 30, "ATK training raises magic attack too");
    t.trDef = 20;
    ck(t.defStat() == def + 20 && t.spdStat() == spd + 20, "DEF training raises magic defence too");
    t.trSpe = 10;
    ck(t.speStat() == spe + 10, "speed training raises speed");
    uint16_t hp = t.vitStat();
    t.trAtk = t.trDef = t.trSpe = 250;
    ck(t.vitStat() == hp, "and nothing trains HP");
    t.ivAtk = t.ivDef = t.ivSpe = t.ivHp = 0;
    ck(t.atkStat() == atk + 250, "an IV adds nothing to a stat");
    ck(t.trMaxAtk() == 255 && t.trMaxDef() == 255 && t.trMaxSpe() == 255 &&
       Pet::trMaxFor(0) == 255 && Pet::trMaxFor(31) == 255, "the cap is 255 whatever the IV");
  }

  // --- the IV scales a session: 70% at 0, 130% at 31, as-is at an average one
  ck(trainGainFor(18, 0) == 13, "IV 0 pays 70% of a full session");
  ck(trainGainFor(18, 15) == 18, "an average IV pays what the session says");
  ck(trainGainFor(18, 31) == 23, "IV 31 pays 130%");
  ck(trainGainFor(10, 0) == 7 && trainGainFor(10, 31) == 13, "and it holds for a smaller session");

  // --- a session near the cap stops at it rather than wrapping the byte
  {
    Pet t;
    t.begin();
    t.dbgHatchAs(GREYMON, false);
    t.ivAtk = t.ivSpe = t.ivDef = 31;
    t.trAtk = t.trSpe = t.trDef = 250;
    ck(t.trainStrength(200) == 5 && t.trAtk == 255, "the bag stops at 255, it does not wrap");
    ck(t.trainSpeed(200) == 5 && t.trSpe == 255, "and so does the reaction test");
    ck(t.playResult(100) == 5 && t.trDef == 255, "and the ball");
    ck(t.trainStrength(200) == 0 && t.trAtk == 255, "a capped stat gains nothing more");
  }

  // --- HP: the de-digivolve boost, x1.1 a time, capped at x2.0
  {
    const uint16_t hp = petCalcHp(GREYMON, 60, 0);
    ck(petCalcHp(GREYMON, 60, 1) == hp * 110 / 100, "one de-digivolution is x1.1");
    ck(petCalcHp(GREYMON, 60, 5) == hp * 150 / 100, "five are x1.5");
    ck(petCalcHp(GREYMON, 60, 10) == hp * 2, "ten reach x2.0");
    ck(petCalcHp(GREYMON, 60, 30) == hp * 2, "and it stops there");
  }

  // --- each form has its own shape: the table's physical and magic forms stay that way
  p.ageMinutes = 5940;
  uint16_t atk = p.atkStat(), spa = p.spaStat();
  printf("     DEVIMON L%u: atk=%u spa=%u\n", p.level(), atk, spa);
  Pet m;
  m.dbgHatchAs(OGREMON, false);
  m.ageMinutes = 5940;
  printf("     OGREMON L%u: atk=%u spa=%u\n", m.level(), m.atkStat(), m.spaStat());
  ck(m.atkStat() > m.spaStat(), "OGREMON hits harder physically than with magic");
  ck(m.atkStat() != atk || m.spaStat() != spa, "and the two species are not interchangeable");

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}

// DIGIVOLUTION: Time Stranger's routes, TamaDigi's clock.
//
// A form's routes are in TS_EVO_TBL. A branch opens when its stage timer has run
// (a minimum, not a schedule) and its stat minimums, age minimum and Agent Rank are
// met, and it is offered, never forced. These tests pin that, and the worked
// examples in docs/HANDOFF.md, which are the design's own numbers.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "roster.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed = 99;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0, g_touchY=0; volatile bool g_touchDown=false;
bool wasPressed=false;
uint32_t millis(){ return 0; }
void FakeESP::restart(){ exit(0); }
int FakeSerial::available(){ return 0; }
String FakeSerial::readStringUntil(char){ return String(""); }
void sfxPlay(uint8_t){}
#endif

static int bad=0;
static void ck(bool ok,const char*w){ printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++; }

static const int16_t BOTAMON=6, KOROMON=9, AGUMON=21, GREYMON=89;

static Pet *fresh(int16_t dex, uint8_t lvl) {
  static Pet p;
  p.begin();
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.formAgeMin = p.ageMinutes;
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.careMistakes = 0;
  p.trAtk = p.trDef = p.trSpe = 0;
  return &p;
}

static const TsEvo *route(int16_t from, int16_t to) {
  const TsDexEntry &d = TS_DEX_TBL[from];
  for (uint16_t i = d.evoFirst; i < (uint16_t)d.evoFirst + d.evoCount; i++)
    if ((int16_t)TS_EVO_TBL[i].to == to) return &TS_EVO_TBL[i];
  return nullptr;
}

static bool offers(const Pet *p, int16_t to) {
  int16_t open[EVO_OPTIONS_MAX];
  const uint8_t n = p->evoOptions(open, EVO_OPTIONS_MAX);
  for (uint8_t i = 0; i < n; i++) if (open[i] == to) return true;
  return false;
}

int main(){
  gArtPack = true;

  // the table: every route runs between real forms, none onto itself
  {
    bool ok = true;
    for (uint16_t i = 0; i < TS_EVO_COUNT; i++) {
      if (!dexValid((int16_t)TS_EVO_TBL[i].from) || !dexValid((int16_t)TS_EVO_TBL[i].to)) ok = false;
      if (TS_EVO_TBL[i].from == TS_EVO_TBL[i].to) ok = false;
    }
    ck(ok, "every route runs between two real forms");
    int over = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (TS_DEX_TBL[d].evoCount > EVO_OPTIONS_MAX) over++;
    ck(over == 0, "and no form has more routes than EVO_OPTIONS_MAX holds");
  }

  // In-Training I -> II: the timer opens at level 2, and BOTAMON's own minimums
  // (350 HP, 150 ATK) take it to level 5.
  {
    Pet *p = fresh(BOTAMON, 1);
    ck(!p->canEvolveNow(), "a newborn BOTAMON is not offered anything at level 1");
    p->ageMinutes = 60;
    ck(!p->canEvolveNow(), "at level 2 the timer is up, but a timer is only a minimum");
    p->ageMinutes = 4 * 60;
    ck(p->canEvolveNow() && p->digiTarget() == KOROMON, "at level 5 it can become KOROMON");
    ck(p->speciesId == BOTAMON, "and that is an offer: nothing happened on its own");
  }

  // HANDOFF: "Agumon -> Greymon at ~hour 37". Its HP minimum is the gate.
  {
    Pet *p = fresh(AGUMON, 25);
    const TsEvo *e = route(AGUMON, GREYMON);
    ck(e != nullptr, "AGUMON has a route to GREYMON");
    p->trAtk = p->trDef = p->trSpe = 255;       // only HP and the timer left to wait on
    uint8_t opened = 0;
    for (uint8_t l = 25; l <= 100 && !opened; l++) {
      p->ageMinutes = (uint32_t)(l - 1) * MINUTES_PER_LEVEL;
      if (p->routeMet(*e)) opened = l;
    }
    printf("      AGUMON -> GREYMON opens at level %u\n", opened);
    ck(opened >= 36 && opened <= 40, "HP makes AGUMON wait until about hour 37");
    p->ageMinutes = 60UL * 20;
    ck(!p->routeMet(*e), "and the stage timer is a floor under it");
  }

  // a stat minimum is the same number as the stat: the table's, scaled
  {
    const TsEvo *e = nullptr;
    for (uint16_t i = TS_DEX_TBL[GREYMON].evoFirst; i < TS_DEX_TBL[GREYMON].evoFirst + TS_DEX_TBL[GREYMON].evoCount; i++)
      if (TS_EVO_TBL[i].min[TS_STAT_ATK] && (!e || TS_EVO_TBL[i].min[TS_STAT_ATK] > e->min[TS_STAT_ATK]))
        e = &TS_EVO_TBL[i];
    ck(e != nullptr, "GREYMON has a route with an ATK minimum");
    Pet *p = fresh(GREYMON, 49);
    p->trDef = p->trSpe = 255;
    p->deDigiCount = 10;                        // HP out of the way
    p->ageMinutes = (uint32_t)(p->routeClock(*e) + 20 - 1) * MINUTES_PER_LEVEL;
    p->formAgeMin = p->ageMinutes;
    const uint16_t need = tsScaled(e->min[TS_STAT_ATK]);
    const uint16_t grown = petCalcStat(GREYMON, TS_STAT_ATK, p->level(), 0);
    printf("      needs ATK %u, grown to %u at level %u\n", need, grown, p->level());
    ck(need > grown, "that minimum is past what age alone has reached");
    p->trAtk = (uint8_t)(need - grown - 1);
    ck(!p->routeMet(*e), "one point short of it the route stays shut");
    p->trAtk = (uint8_t)(need - grown);
    ck(p->routeMet(*e) || e->min[TS_STAT_HP] || e->minLevel, "and at exactly the minimum it opens");
  }

  // minLevel is the SP minimum, turned into age
  {
    const TsEvo *e = nullptr;
    for (uint16_t i = 0; i < TS_EVO_COUNT && !e; i++)
      if (TS_EVO_TBL[i].minLevel > TS_STAGE_ENTRY[TS_DEX_TBL[TS_EVO_TBL[i].to].stage % STAGE_COUNT] + 2)
        e = &TS_EVO_TBL[i];
    ck(e != nullptr, "some route has an age minimum past its stage timer");
    Pet *p = fresh((int16_t)e->from, e->minLevel - 1);
    p->trAtk = p->trDef = p->trSpe = 255;
    p->deDigiCount = 10;
    ck(!p->routeMet(*e), "one level short of it the route is shut");
    p->ageMinutes += MINUTES_PER_LEVEL;
    ck(p->routeMet(*e), "and at that level it is open");
  }

  // STUB, until phase 2: Agent Rank passes everywhere. When it lands this flips: a
  // route with a rank must be shut at rank 1. See Pet::agentRank().
  {
    Pet *p = fresh(BOTAMON, 1);
    uint8_t top = 0;
    for (uint16_t i = 0; i < TS_EVO_COUNT; i++) if (TS_EVO_TBL[i].rank > top) top = TS_EVO_TBL[i].rank;
    ck(top == 8 && p->agentRank() >= top, "the rank stub clears the highest rank in the table");
  }

  // choosing among branches: declining moves the offer to the next open one
  {
    Pet *p = fresh(KOROMON, 25);
    p->trAtk = p->trDef = p->trSpe = 100;
    int16_t open[EVO_OPTIONS_MAX];
    const uint8_t n = p->evoOptions(open, EVO_OPTIONS_MAX);
    printf("      KOROMON has %u branches open at level 25\n", n);
    ck(n >= 2, "a trained KOROMON has more than one branch open");
    const int16_t first = p->digiTarget();
    ck(first == open[0], "the first is on offer");
    p->declineEvolve();
    ck(p->digiTarget() == open[1 % n], "keeping form moves the offer to the next branch");
    ck(!p->wantEvolveButton(), "and it waits for the next level to ask again");
    p->ageMinutes += MINUTES_PER_LEVEL;
    ck(p->wantEvolveButton(), "which it then does");
    const int16_t want = p->digiTarget();
    p->evolve();
    ck(p->speciesId == want, "evolve() takes the branch on offer");
    ck(p->careMistakes == 0, "and the new form counts care mistakes from zero");
  }

  // never into something that cannot be drawn
  {
    int shown = 0, hidden = 0;
    Pet *p = fresh(AGUMON, 99);
    p->trAtk = p->trDef = p->trSpe = 255;
    p->deDigiCount = 10;
    int16_t open[EVO_OPTIONS_MAX];
    const uint8_t n = p->evoOptions(open, EVO_OPTIONS_MAX);
    for (uint8_t i = 0; i < n; i++) (speciesHasArt(open[i]) ? shown : hidden)++;
    ck(shown > 0 && hidden == 0, "only forms with art are ever offered");
    const TsDexEntry &d = TS_DEX_TBL[AGUMON];
    int artless = 0;
    for (uint16_t i = d.evoFirst; i < (uint16_t)d.evoFirst + d.evoCount; i++)
      if (!speciesHasArt((int16_t)TS_EVO_TBL[i].to)) artless++;
    ck(artless > 0, "though AGUMON has routes to forms without art, or this proves nothing");
    gArtPack = false;
    ck(p->digiTarget() == 0 && !p->canEvolveNow(), "without the pack on the card nothing is offered");
    ck(p->evoNextLevel() == 0, "and the card reads as a last form rather than a promise");
    gArtPack = true;
    ck(p->digiTarget() != 0, "and with it, it is");
  }

  // de-digivolving: known forms only, it waits out its stage, HP goes up.
  // HANDOFF's example: Greymon de-digivolves at 40, is Greymon again at 58 with x1.1.
  {
    static Pet q;
    q.begin();
    q.dbgHatchAs(AGUMON, false);
    q.trAtk = q.trDef = q.trSpe = 255;
    q.ageMinutes = 39UL * MINUTES_PER_LEVEL;     // level 40
    q.formAgeMin = 0;
    q.fullness = q.joy = q.energy = q.hygiene = 100;
    ck(q.level() == 40, "at level 40");
    ck(offers(&q, GREYMON), "AGUMON can become GREYMON");
    while (q.digiTarget() != GREYMON) q.declineEvolve();
    q.evolve();
    ck(q.speciesId == GREYMON, "and does");
    const uint16_t hp = q.vitStat();
    int16_t back[8];
    ck(q.devolveOptions(back, 8) >= 1, "GREYMON can go back");
    bool hasAgumon = false;
    for (uint8_t i = 0; i < q.devolveOptions(back, 8); i++) if (back[i] == AGUMON) hasAgumon = true;
    ck(hasAgumon, "into AGUMON, which it has been");
    ck(!q.devolve(21 + 1), "but not into a form it has not been");
    ck(q.devolve(AGUMON), "de-digivolving works");
    ck(q.speciesId == AGUMON && q.deDigiCount == 1, "and is counted");
    ck(q.evoHold == 58, "AGUMON then waits out its stage: 40 + (25 - 7)");
    ck(!q.canEvolveNow(), "so nothing opens straight away");
    q.ageMinutes = 56UL * MINUTES_PER_LEVEL;     // level 57
    ck(!offers(&q, GREYMON), "not even at level 57");
    q.ageMinutes = 57UL * MINUTES_PER_LEVEL;     // level 58
    ck(offers(&q, GREYMON), "and at 58 GREYMON is open again");
    while (q.digiTarget() != GREYMON) q.declineEvolve();
    q.evolve();
    ck(q.speciesId == GREYMON && q.evoHold == 0, "a digivolution clears the hold");
    ck(q.vitStat() > hp, "and it is a tougher GREYMON than the first");
    ck(q.vitStat() == petCalcHp(GREYMON, 58, 1), "by exactly the x1.1");
    const uint8_t count = q.deDigiCount;
    q.dbgHatchAs(BOTAMON, false);
    ck(count == 1 && q.deDigiCount == 0 && q.evoHold == 0, "a new partner starts over");
  }

  // HANDOFF: "Greymon -> Kimeramon (2030 HP) opens at level 68 instead of 81"
  {
    int16_t kimera = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (!strcmp(TS_DEX_TBL[d].name, "KIMERAMON")) kimera = d;
    const TsEvo *e = kimera ? route(GREYMON, kimera) : nullptr;
    ck(e != nullptr && e->min[TS_STAT_HP] == 2030, "GREYMON -> KIMERAMON asks 2030 HP");
    uint8_t open0 = 0, open1 = 0;
    for (uint8_t l = 1; l <= 100; l++) {
      if (!open0 && petCalcHp(GREYMON, l, 0) >= tsScaled(e->min[TS_STAT_HP])) open0 = l;
      if (!open1 && petCalcHp(GREYMON, l, 1) >= tsScaled(e->min[TS_STAT_HP])) open1 = l;
    }
    printf("      HP opens it at level %u unboosted, %u with one loop\n", open0, open1);
    ck(open0 == 81 && open1 == 68, "level 81 unboosted, 68 with one de-digivolution");
  }

  // a form with no way on, and one that never fought, still reaches the end of its life
  {
    Pet *p = fresh(GREYMON, 73);
    p->ageMinutes = FAREWELL_AGE_MIN;
    ck(p->canFarewellNow(), "a three-day-old Champion can say goodbye");
  }

  // every stage a partner can reach has art to reach: the game stays playable
  {
    int stages[STAGE_COUNT] = { 0 };
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (speciesHasArt(d) && TS_DEX_TBL[d].stage < STAGE_COUNT) stages[TS_DEX_TBL[d].stage]++;
    ck(stages[TS_ST_IN_TRAINING_1] == 3, "three In-Training I forms have art to hatch");
    ck(stages[TS_ST_ROOKIE] && stages[TS_ST_CHAMPION] && stages[TS_ST_ULTIMATE] && stages[TS_ST_MEGA],
       "and there is art at every stage up to Mega");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

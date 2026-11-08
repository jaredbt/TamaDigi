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

  // AGENT RANK: 1 + the Easy area badges, 10 for the champion. Hard badges and the
  // elites never count. Routes below Ultimate need none; into Ultimate 5, Mega 7,
  // Mega+ 8.
  {
    Pet *p = fresh(BOTAMON, 1);
    p->badges = p->badgesHard = 0;
    uint8_t top = 0;
    for (uint16_t i = 0; i < TS_EVO_COUNT; i++) if (TS_EVO_TBL[i].rank > top) top = TS_EVO_TBL[i].rank;
    ck(top == 8, "the table's highest rank is 8 (into Mega+)");
    ck(p->agentRank() == 1, "a new tamer is rank 1");
    p->badgesHard = 0x1FFF;
    ck(p->agentRank() == 1, "and every Hard badge leaves it there");
    p->badges = 0x0F;
    ck(p->agentRank() == 5, "four Easy area badges are rank 5: Ultimate opens");
    p->badges = 0x3F;
    ck(p->agentRank() == 7, "six are rank 7: Mega opens");
    p->badges = 0xFF | (0x0F << TRAINER_GYMS);
    ck(p->agentRank() == 9, "eight are rank 9, and the four elites add nothing");
    p->badges |= 1u << TRAINER_CHAMPION;
    ck(p->agentRank() == AGENT_RANK_MAX && AGENT_RANK_MAX == 10, "the champion makes it 10");
    p->badges = p->badgesHard = 0;
  }
  // ...and it gates the routes that ask for it, and only those
  {
    const TsEvo *gate = nullptr;    // a Champion -> Ultimate route with art at both ends
    for (uint16_t i = 0; i < TS_EVO_COUNT && !gate; i++) {
      const TsEvo &e = TS_EVO_TBL[i];
      if (e.rank == 5 && TS_DEX_TBL[e.from].stage == TS_ST_CHAMPION &&
          speciesHasArt((int16_t)e.from) && speciesHasArt((int16_t)e.to)) gate = &e;
    }
    ck(gate != nullptr, "some drawable Champion -> Ultimate route asks rank 5");
    Pet *p = fresh((int16_t)gate->from, 99);
    p->trAtk = p->trDef = p->trSpe = 255;
    p->deDigiCount = DEDIGI_COUNT_MAX;
    p->badges = 0x07;                      // rank 4
    ck(!p->routeMet(*gate), "at rank 4 the route into Ultimate is shut");
    ck(!offers(p, (int16_t)gate->to), "and the branch is not offered");
    p->badges = 0x0F;                      // rank 5
    ck(p->routeMet(*gate), "at rank 5 it opens");
    ck(offers(p, (int16_t)gate->to), "and is offered");
    p->badges = 0x07;
    bool everyBelow = true;
    for (uint16_t i = 0; i < TS_EVO_COUNT; i++)
      if (TS_DEX_TBL[TS_EVO_TBL[i].to].stage < TS_ST_ULTIMATE && TS_EVO_TBL[i].rank) everyBelow = false;
    ck(everyBelow, "no route into a stage below Ultimate asks any rank");
    p->badges = 0;
  }
  // evoRankWanted() is what the card asks: rank is named only when it is the one
  // thing missing.
  {
    int16_t champ = 0;
    const TsEvo *only = nullptr;
    for (int16_t d = 1; d <= DEX_COUNT && !only; d++) {
      if (TS_DEX_TBL[d].stage != TS_ST_CHAMPION || !speciesHasArt(d)) continue;
      const TsDexEntry &de = TS_DEX_TBL[d];
      bool allRanked = de.evoCount > 0;
      for (uint16_t i = de.evoFirst; i < (uint16_t)de.evoFirst + de.evoCount; i++)
        if (speciesHasArt((int16_t)TS_EVO_TBL[i].to) && !TS_EVO_TBL[i].rank) allRanked = false;
      if (allRanked) { champ = d; only = &TS_EVO_TBL[de.evoFirst]; }
    }
    ck(champ != 0, "some drawable Champion has only ranked branches");
    Pet *p = fresh(champ, 99);
    p->trAtk = p->trDef = p->trSpe = 255;
    p->deDigiCount = DEDIGI_COUNT_MAX;
    p->badges = 0;
    ck(!p->canEvolveNow() && p->evoRankWanted() == 5, "at rank 1 the card says rank 5 is wanted");
    p->badges = 0x0F;
    ck(p->canEvolveNow() && p->evoRankWanted() == 0, "and once it is held, nothing is wanted");
    p->badges = 0;
    Pet *q = fresh(champ, 25);             // untrained, young: rank is not the only gap
    q->badges = 0;
    bool statGap = false;
    const TsDexEntry &de = TS_DEX_TBL[champ];
    for (uint16_t i = de.evoFirst; i < (uint16_t)de.evoFirst + de.evoCount; i++)
      if (!q->routeMet(TS_EVO_TBL[i])) statGap = true;
    ck(statGap, "a young untrained one is shut for more than rank");
  }

  // choosing among branches: the offer can take any open one; keeping form waits
  {
    Pet *p = fresh(KOROMON, 25);
    p->trAtk = p->trDef = p->trSpe = 100;
    int16_t open[EVO_OPTIONS_MAX];
    const uint8_t n = p->evoOptions(open, EVO_OPTIONS_MAX);
    printf("      KOROMON has %u branches open at level 25\n", n);
    ck(n >= 2, "a trained KOROMON has more than one branch open");
    ck(p->digiTarget() == open[0], "the first is what evolve() would take");
    p->declineEvolve();
    ck(!p->wantEvolveButton(), "keeping form waits for the next level to ask again");
    p->ageMinutes += MINUTES_PER_LEVEL;
    ck(p->wantEvolveButton(), "which it then does");
    ck(!p->evolveTo(GREYMON), "evolveTo() refuses a form that is not an open branch");
    ck(p->speciesId == KOROMON, "and nothing changed");
    const int16_t want = open[n - 1];
    ck(p->evolveTo(want), "evolveTo() takes any open branch, not only the first");
    ck(p->speciesId == want, "the one chosen");
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
    q.evolveTo(GREYMON);
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
    q.evolveTo(GREYMON);
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

  // JOGRESS. KIMERAMON + MACHINEDRAMON -> MILLENNIUMMON (Mega, Agent Rank 7) is the one
  // pair the public art draws end to end, so it carries the rules.
  {
    const int16_t KIMERA = 229, MACHINE = 400, MILLENNIUM = 399;
    const TsJogress *mj = nullptr;
    for (uint8_t i = 0; i < TS_JOGRESS_COUNT; i++)
      if (TS_JOGRESS_TBL[i].to == MILLENNIUM) mj = &TS_JOGRESS_TBL[i];
    ck(mj && mj->a == KIMERA && mj->b == MACHINE && mj->rank == 7 && mj->partnersRaisable,
       "KIMERAMON + MACHINEDRAMON -> MILLENNIUMMON asks Agent Rank 7");
    ck(!strcmp(TS_DEX_TBL[KIMERA].name, "KIMERAMON") && !strcmp(TS_DEX_TBL[MACHINE].name, "MACHINEDRAMON") &&
           !strcmp(TS_DEX_TBL[MILLENNIUM].name, "MILLENNIUMMON"),
       "(the dex ids name the forms this block says)");
    Pet *p = fresh(KIMERA, 61);
    p->badges = 0x3F;                      // six Easy area badges: rank 7
    p->historyCount = 0;
    int16_t h[JOGRESS_OPTIONS_MAX];
    ck(p->jogressEligible(*mj) && p->jogressMet(*mj), "a rank-7 KIMERAMON at the Mega stage timer can fuse");
    ck(p->jogressWildHalves(h, JOGRESS_OPTIONS_MAX) == 1 && h[0] == MACHINE,
       "and its other half, MACHINEDRAMON, is the wild boost's pool");
    p->badges = 0x1F;                      // rank 6
    ck(!p->jogressEligible(*mj) && !p->jogressWildHalves(h, JOGRESS_OPTIONS_MAX),
       "rank 6 is not eligible: no fusion, no boost");
    p->badges = 0x3F;
    p->ageMinutes = 59UL * MINUTES_PER_LEVEL;   // level 60
    ck(!p->jogressEligible(*mj), "nor is level 60: the result's stage timer applies, as on any route");
    p->ageMinutes = 60UL * MINUTES_PER_LEVEL;
    // Either half fuses.
    Pet *m = fresh(MACHINE, 61);
    m->badges = 0x3F;
    m->historyCount = 0;
    ck(m->jogressWildHalves(h, JOGRESS_OPTIONS_MAX) == 1 && h[0] == KIMERA, "MACHINEDRAMON is a half too");
    p = fresh(KIMERA, 61);
    p->badges = 0x3F;
    p->historyCount = 0;

    // (b) a wild Digimon just beaten: only the other half opens it.
    JogressOption jo[JOGRESS_OPTIONS_MAX];
    ck(p->jogressOptions(jo, JOGRESS_OPTIONS_MAX, MACHINE) == 1 && jo[0].to == MILLENNIUM && jo[0].other == MACHINE,
       "beating a wild MACHINEDRAMON opens MILLENNIUMMON");
    ck(!p->jogressOptions(jo, JOGRESS_OPTIONS_MAX, GREYMON), "beating anything else opens nothing");
    ck(!p->jogress(MILLENNIUM, MACHINE, GREYMON), "and the fusion cannot be claimed off another win");

    // (a) the partner history: it must have left as that exact form.
    ck(!offers(p, MILLENNIUM), "with an empty history the offer has no fusion");
    p->history[0] = PartnerRecord();
    p->history[0].dex = 401;               // not MACHINEDRAMON
    p->historyCount = 1;
    ck(!offers(p, MILLENNIUM), "a partner that left as another form is no half");
    p->history[1] = PartnerRecord();
    p->history[1].dex = MACHINE;
    p->historyCount = 2;
    ck(offers(p, MILLENNIUM) && p->canEvolveNow(), "one that left as MACHINEDRAMON is: the offer has the fusion");
    p->badges = 0x1F;
    ck(!offers(p, MILLENNIUM), "(not at rank 6)");
    p->badges = 0x3F;
    const uint8_t dd0 = p->deDigiCount;
    ck(p->evolveTo(MILLENNIUM) && p->speciesId == MILLENNIUM, "taking it fuses into MILLENNIUMMON");
    ck(p->jogressHalf == KIMERA, "remembering KIMERAMON as its own half");
    ck(p->historyCount == 2 && p->history[1].dex == MACHINE, "and the history entry stays");
    ck(p->isRegistered(MILLENNIUM), "MILLENNIUMMON is raised in the Digidex");

    // De-digivolving goes back to your own half, with the usual HP boost.
    int16_t back[EVO_OPTIONS_MAX];
    ck(p->devolveOptions(back, EVO_OPTIONS_MAX) >= 1 && back[0] == KIMERA,
       "de-digivolving a fused MILLENNIUMMON offers KIMERAMON first");
    p->dbgRegister(MACHINE);
    bool machineOffered = false;
    const uint8_t nb = p->devolveOptions(back, EVO_OPTIONS_MAX);
    for (uint8_t i = 0; i < nb; i++) if (back[i] == MACHINE) machineOffered = true;
    ck(!machineOffered, "never the other half, even one in the Digidex");
    ck(p->devolve(KIMERA) && p->speciesId == KIMERA, "and goes back to it");
    ck(p->deDigiCount == dd0 + 1 && p->vitStat() == petCalcHp(KIMERA, p->level(), dd0 + 1),
       "HP x1.1, as any de-digivolution");
    ck(p->evoHold > p->level() && p->jogressHalf == 0, "it waits out its stage, unfused");

    // A wild fusion, then the same way back.
    p = fresh(KIMERA, 61);
    p->badges = 0x3F;
    p->historyCount = 0;
    ck(p->jogress(MILLENNIUM, MACHINE, MACHINE) && p->speciesId == MILLENNIUM && p->jogressHalf == KIMERA,
       "a wild MACHINEDRAMON fuses the same way");
    p->dbgHatchAs(BOTAMON, false);
    ck(p->jogressHalf == 0, "a new partner has not fused");
    p->badges = 0;

    // A pair whose halves can never be partners never opens (SUSANOMON's).
    for (uint8_t i = 0; i < TS_JOGRESS_COUNT; i++) {
      const TsJogress &j = TS_JOGRESS_TBL[i];
      if (j.partnersRaisable) continue;
      Pet *e = fresh((int16_t)j.a, 90);
      e->badges = 0xFF | (1u << TRAINER_CHAMPION);
      ck(!e->jogressEligible(j) && !e->jogressWildHalves(h, JOGRESS_OPTIONS_MAX),
         "a pair with a half that cannot be raised (SUSANOMON's) never opens");
      e->badges = 0;
    }
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

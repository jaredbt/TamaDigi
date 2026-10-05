// DIGIVOLUTION: the branch a Digimon takes is decided by how it was raised.
//
// EVO_TBL lists the branches out of each form in priority order, and
// Pet::digiTarget() takes the first one whose conditions hold. These tests pin
// the V-Pet pattern on Ver.1 (Koromon -> Agumon/Betamon, Agumon -> five
// Champions by care mistakes and training, Champion -> Ultimate only after
// battles won) and the guards around it: a stage clock, a fresh count per stage,
// and never a target that cannot be drawn.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "dex.h"
#include "noart.h"
#include "moves.h"
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


static Pet *fresh(int16_t dex, uint8_t lvl) {
  static Pet p;
  p.begin();
  p.dbgHatchAs(dex, false);
  p.ageMinutes = (uint32_t)(lvl - 1) * MINUTES_PER_LEVEL;
  p.fullness = p.joy = p.energy = p.hygiene = 100;
  p.careMistakes = 0; p.wins = 0;
  p.trAtk = p.trDef = p.trSpe = 0;
  return &p;
}

int main(){
  gRegionArt = 0xFFFF;

  // the table itself: every branch climbs exactly one stage
  {
    bool ok = true;
    for (int i = 0; i < EVO_COUNT; i++)
      if (DEX_TBL[EVO_TBL[i].to].stage != DEX_TBL[EVO_TBL[i].from].stage + 1) ok = false;
    ck(ok, "every branch goes up exactly one stage");
    bool hatchOnly = true;
    for (int i = 0; i < EVO_COUNT; i++)
      if (DEX_TBL[EVO_TBL[i].to].rarity != R_EVO) hatchOnly = false;
    ck(hatchOnly, "nothing a branch leads to can hatch from an egg");
  }

  // Baby I -> Baby II on the clock, not before
  {
    Pet *p = fresh(1, 1);
    ck(!p->canEvolveNow(), "a newborn BOTAMON is not offered anything at level 1");
    p->ageMinutes = 60;
    ck(p->canEvolveNow() && p->digiTarget() == 2, "at level 2 it becomes KOROMON");
  }

  // Koromon: few mistakes -> Agumon, more -> Betamon
  {
    Pet *p = fresh(2, 7);
    ck(p->digiTarget() == 3, "KOROMON with no care mistakes becomes AGUMON");
    p->careMistakes = 4;
    ck(p->digiTarget() == 4, "and with four it becomes BETAMON");
  }

  // Agumon's five Champions
  {
    Pet *p = fresh(3, 25);
    p->trAtk = 30; p->trDef = 15;           // 45 training
    ck(p->digiTarget() == 5, "well cared for and trained: GREYMON");
    p->trAtk = 10; p->trDef = 0;
    ck(p->digiTarget() == 6, "well cared for, untrained: TYRANNOMON");
    p->careMistakes = 4; p->trAtk = 40;
    ck(p->digiTarget() == 7, "some mistakes, trained: DEVIMON");
    p->trAtk = 5;
    ck(p->digiTarget() == 8, "some mistakes, untrained: MERAMON");
    p->careMistakes = 9;
    ck(p->digiTarget() == 11, "neglected: NUMEMON");
    p->ageMinutes = 60UL * 20;
    ck(!p->canEvolveNow(), "but none of it before the Rookie's day is up");
  }

  // Champion -> Ultimate needs wins, and the card can say how many
  {
    Pet *p = fresh(5, 49);
    ck(p->digiTarget() == 0 && !p->canEvolveNow(), "a GREYMON with no wins stays a GREYMON");
    ck(p->winsNeeded() == 5, "and it needs five");
    for (int i = 0; i < 5; i++) p->addWin();
    ck(p->wins == 5, "five wins are counted");
    ck(p->canEvolveNow() && p->digiTarget() == 12, "then METALGREYMON is open");
    p->careMistakes = 3;
    p->evolve();
    ck(p->speciesId == 12, "evolve() takes it");
    ck(p->wins == 0 && p->careMistakes == 0, "and the new stage counts from zero");
    ck(DEX_TBL[p->speciesId].evolvesTo == 15 && p->winsNeeded() == 15,
       "and from there BLITZGREYMON waits behind fifteen more wins");
  }

  // a Champion that never fought still reaches the end of its life
  {
    Pet *p = fresh(5, 73);
    p->ageMinutes = FAREWELL_AGE_MIN;
    ck(p->canFarewellNow(), "a three-day-old Champion can say goodbye");
  }

  // the signature attack arrives with the new form, even if postponed
  {
    Pet *p = fresh(2, 9);                  // two hours past the Rookie gate
    p->checkLearnGates();
    p->evolve();
    bool knows = false;
    for (int i = 0; i < MOVE_SLOTS; i++) if (p->moves[i] == MV_PEPPER_BRTH) knows = true;
    ck(p->speciesId == 3 && knows, "a late-evolving AGUMON still learns PEPPER BREATH");
  }

  // never into something that cannot be drawn: with Ver.1's sprite pack
  // missing, a GREYMON is offered nothing rather than a dex number
  {
    Pet *p = fresh(5, 49);
    for (int i = 0; i < 5; i++) p->addWin();
    gRegionArt = 0xFFFF & ~(uint16_t)(1u << regionOfDex(12));
    ck(p->digiTarget() == 0 && !p->canEvolveNow(), "without Ver.1's pack nothing is offered");
    gRegionArt = 0xFFFF;
    ck(p->digiTarget() == 12, "and with it, METALGREYMON");
  }

  // every version's tree runs all the way to Mega
  {
    bool ok = true;
    for (uint8_t r = 0; r < REGION_COUNT - 1; r++) {
      bool mega = false;
      for (int16_t d = REGIONS[r].lo; d <= REGIONS[r].hi; d++)
        if (DEX_TBL[d].stage == ST_MEGA) mega = true;
      if (!mega) ok = false;
    }
    ck(ok, "every version has Mega forms to reach");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

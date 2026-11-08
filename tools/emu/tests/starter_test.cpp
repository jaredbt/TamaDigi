// First boot: no chooser. The first egg hatches a random In-Training I that can be
// drawn (on public art PUNIMON, BOTAMON or POYOMON), and the panel goes straight to
// that egg. A replaced v1.0.0 save is announced on the egg and cleared at hatch.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>
#include <cstring>
#include <set>
uint32_t g_seed=29; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void FakeESP::restart(){exit(0);}

void setup(); void render(); void onTap(int16_t x, int16_t y);
uint8_t uiCurrentScreen();
extern bool gOldSaveReplaced;
extern Pet pet;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static bool hatchling(int16_t d) {
  int16_t forms[8];
  const uint8_t n = starterForms(forms, 8);
  for (uint8_t i = 0; i < n; i++) if (forms[i] == d) return true;
  return false;
}

int main(){
  setup();
  render();
  ck(pet.isEgg(), "a brand new save is an egg");
  ck(hatchling(pet.eggPeek()), "of an In-Training I that can be drawn");
  ck(TS_DEX_TBL[pet.eggPeek()].stage == TS_ST_IN_TRAINING_1, "and it is an In-Training I");

  // The first egg is uniform over the hatchlings: every one turns up, none other does.
  {
    std::set<int16_t> got;
    bool stray = false;
    for (int i = 0; i < 300; i++) {
      Pet p;                      // a Pet that never opened: an empty Digidex
      const int16_t d = p.pickEggSpecies();
      if (!hatchling(d)) stray = true;
      got.insert(d);
    }
    int16_t forms[8];
    ck(!stray, "a first egg is never anything but a drawable In-Training I");
    ck(got.size() == starterForms(forms, 8), "and every one of them can come out");
  }

  // The old-save notice is on the egg, and goes when the egg hatches.
  gOldSaveReplaced = true;
  render();
  ck(gOldSaveReplaced, "the old-save notice stays while the egg is on the panel");
  for (int i = 0; i < 3 && pet.isEgg(); i++) onTap(233, 233);
  ck(!pet.isEgg(), "three taps hatch it, with nothing to choose first");
  render();
  ck(!gOldSaveReplaced, "and the old-save notice has done its job");

  // A reload does not bring a chooser back, and nor does a wipe.
  {
    Pet again; again.begin();
    ck(!again.isEgg() && again.speciesId == pet.speciesId, "the hatched partner reloads as itself");
  }
  {
    pet.factoryReset();
    Pet fresh; fresh.begin();
    ck(fresh.isEgg() && hatchling(fresh.eggPeek()), "a wiped save is a new random egg");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

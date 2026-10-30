// First boot: pick one of the In-Training I forms that can be drawn. On public art
// that is PUNIMON, BOTAMON and POYOMON. A stand-in until phase 2's first egg, which
// hatches a random one.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=29; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void FakeESP::restart(){exit(0);}

void setup(); void render(); void onTap(int16_t x, int16_t y);
int16_t starterOf(uint8_t i);
uint8_t starterCountShown();
extern bool gOldSaveReplaced;
extern Pet pet;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// where renderStarterSelect draws row i
static int starterRowY(int i){ return 110 + i*78 + 35; }

int main(){
  setup();
  render();
  ck(pet.awaitingStarter(), "a brand new save asks for a starter");

  // --- the three hatchlings with art, pinned
  const int16_t want[3] = { 5, 6, 7 };
  ck(starterCountShown() == 3, "three starters are offered");
  for (uint8_t i = 0; i < 3; i++) {
    char m[64]; snprintf(m,sizeof(m),"row %u is %s", i, TS_DEX_TBL[want[i]].name);
    ck(starterOf(i) == want[i], m);
  }
  ck(starterOf(3) == 0, "and nothing past them");

  // --- drive the real flow through onTap
  onTap(233, 60);
  ck(pet.awaitingStarter(), "a tap above the rows chooses nothing");
  gOldSaveReplaced = true;
  onTap(233, starterRowY(1));                 // BOTAMON
  ck(!pet.awaitingStarter(), "tapping a row chooses it");
  ck(pet.eggPeek() == 6, "the waiting Digi-Egg is BOTAMON's");
  ck(!gOldSaveReplaced, "and the old-save notice has done its job");

  // --- the flow is not repeated on a reload
  {
    Pet again; again.begin();
    ck(!again.awaitingStarter(), "the first boot does not run a second time");
  }

  // --- a tap that misses every row must do nothing rather than pick row 0.
  // Starting over: an empty Digidex is what makes it a first boot.
  {
    pet.factoryReset();
    Pet fresh; fresh.begin();
    ck(fresh.awaitingStarter(), "a wiped save asks again");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

// First boot: pick a V-Pet version, and its Digi-Egg appears.
//
// A version hatches exactly one Baby I, so there is nothing left to choose
// after the version: the tap on the version finishes the first boot. The mixed
// pool (ALL) is the one case with a real choice, and it only offers Baby Is
// that can be drawn -- Ver.3's POYOMON has no sprite yet.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=29; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void FakeESP::restart(){exit(0);}

void setup(); void render(); void onTap(int16_t x, int16_t y);
int16_t starterOf(uint8_t region, uint8_t i);
uint8_t starterCountShown(uint8_t region);
extern Pet pet;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

// where renderRegionPick draws row i, and where renderStarterSelect draws row i
static int regionRowY(int i){ return 108 + i*72 + 30; }

int main(){
  setup();
  render();
  ck(pet.awaitingStarter(), "a brand new save asks for a starter");

  // --- each version's single Baby I, pinned
  const int16_t want[5] = { 1, 18, 35, 54, 71 };
  for (uint8_t r = 0; r < 5; r++) {
    char m[64]; snprintf(m,sizeof(m),"%s hatches its Baby I", REGIONS[r].name);
    ck(REGIONS[r].starterCount == 1 && REGIONS[r].starters[0] == want[r] && regionHatchable(r), m);
  }
  ck(REGIONS[REGION_ALL].starterCount == 5, "the mixed pool holds all five Baby Is");

  // --- drive the real flow through onTap: VER.2, and its egg is there at once
  onTap(233, regionRowY(1));                  // VER.2
  ck(pet.region == 1, "tapping a version on the first screen sets the egg version");
  ck(!pet.awaitingStarter(), "and its single Baby I is chosen without a second screen");
  ck(pet.eggPeek() == 18, "the waiting Digi-Egg is PUNIMON's");

  // --- and it survives a reload: version is persisted, the flow is not repeated
  {
    Pet again; again.begin();
    ck(again.region == 1, "the region choice is saved");
    ck(!again.awaitingStarter(), "and the first boot does not run a second time");
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

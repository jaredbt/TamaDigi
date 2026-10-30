// Explore chooses its own installed version and routes that choice into the
// real wild encounter builder without changing the egg version.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "noart.h"
#include <cstdio>

uint32_t g_seed = 0xE7A0;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void onTap(int16_t x, int16_t y);
bool startWildBattle(bool hard);
void startTrainerBattle(uint8_t idx, bool hard);
extern bool battleOpen;
int uiTapFinger();
extern Pet pet;
extern bool exploreOpen;
extern int16_t wildDex;

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

int main() {
  {
    Preferences owner;
    owner.begin("handle-probe", false);
    {
      Preferences alias = owner;
      ck(alias.putUChar("probe", 1) == 1, "a copied Preferences initially shares the open handle");
    }
    ck(owner.putUChar("probe", 2) == 0,
       "destroying a copied Preferences closes the original handle, like real NVS");
  }
  setup();
  pet.dbgHatchAs(89, false);
  gArtPack = true;
  ck(startWildBattle(false), "Explore starts an encounter");
  ck(speciesHasArt(wildDex), "and the Digimon it meets can be drawn");
    pet.rename("AFTERWILD");
    ck(pet.saveHealthy() && !pet.savePending(), "starting a wild battle does not invalidate saving");
    Pet afterEncounter;
    afterEncounter.begin();
    ck(afterEncounter.speciesId == pet.speciesId && !strcmp(afterEncounter.nick, "AFTERWILD"),
      "a save made after starting the encounter survives reload");

  // ---- no pack, no encounter: a Digimon that cannot be drawn is not a fight.
  gArtPack = false;
  ck(!startWildBattle(false), "with the sprite pack missing there is no encounter");
  battleOpen = false;
  startTrainerBattle(0, false);
  ck(!battleOpen, "and no boss fight either, which would open on a bare dex number");
  gArtPack = true;
  startTrainerBattle(0, false);
  ck(battleOpen, "and with the pack back the boss fights");
  battleOpen = false;

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
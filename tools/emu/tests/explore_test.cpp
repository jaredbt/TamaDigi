// Explore chooses its own installed version and routes that choice into the
// real wild encounter builder without changing the egg version.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
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
extern bool btlOver, btlWon;
extern uint8_t btlMsgCount;
extern Combatant btlYou, btlFoe;
extern uint8_t choiceKind;
extern uint32_t choiceUntil;
extern int16_t jogWildDex;
void render();
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);

static int bad = 0;
static void ck(bool ok, const char *what) {
  printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) bad++;
}

// Explore until the wild Digimon is `want` (or, with `other`, anything but it), then
// win the fight through the real tap handler: the foe on 1 HP, the basic attack
// (menu cell 2), and every line of narration tapped away until the fight closes.
// A foe that runs, or a miss that lets it, is another encounter.
static bool winWild(int16_t want, bool other, int tries) {
  for (int t = 0; t < tries; t++) {
    battleOpen = false;
    if (!startWildBattle(false)) return false;
    if ((wildDex == want) == other) continue;
    for (int k = 0; k < 200 && battleOpen; k++) {
      if (btlMsgCount || btlOver) { onTap(233, 320); continue; }
      btlFoe.hp = 1;
      btlYou.hp = btlYou.maxHp;
      onTap(69 + 40, 286 + 52 + 20);
    }
    if (!battleOpen && btlWon) return true;
  }
  return false;
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

  // ---- Jogress after a wild win. A rank-7 KIMERAMON at level 61 can fuse with a
  // wild MACHINEDRAMON into MILLENNIUMMON: beating one asks, anything else does not.
  {
    const int16_t KIMERA = 229, MACHINE = 400, MILLENNIUM = 399;
    pet.dbgHatchAs(KIMERA, false);
    pet.ageMinutes = 60UL * MINUTES_PER_LEVEL;
    pet.formAgeMin = pet.ageMinutes;
    pet.badges = 0x3F;
    pet.historyCount = 0;
    while (pet.hasLearnOffer()) pet.declineLearn();
    int b1t, b1b, b2t, b2b;
    uiConfirmRects(&b1t, &b1b, &b2t, &b2b);

    // About 1 battle in 5 meets the other half on top of the ordinary roll (which, on
    // the public art's short Mega pool, meets MACHINEDRAMON fairly often by itself).
    // wild_test pins the roll; this is the encounter builder using it.
    auto meets = [&](int n) {
      int m = 0;
      for (int i = 0; i < n; i++) {
        battleOpen = false;
        if (startWildBattle(false) && wildDex == MACHINE) m++;
      }
      battleOpen = false;
      return m;
    };
    pet.badges = 0x1F;                     // rank 6: not eligible
    const int base = meets(400);
    pet.badges = 0x3F;
    const int met = meets(400);
    printf("      MACHINEDRAMON in %d of 400 encounters at rank 6, %d at rank 7\n", base, met);
    ck(met - base >= 50 && met - base <= 110,
       "an eligible partner meets its fusion's other half in about 1 more battle in 5");

    ck(winWild(MACHINE, true, 50) && choiceKind == 0, "beating anything else is a plain win: no prompt");
    ck(winWild(MACHINE, false, 50), "(won against a wild MACHINEDRAMON)");
    ck(choiceKind == 5 && jogWildDex == MACHINE, "beating MACHINEDRAMON opens the Jogress prompt as the fight closes");
    render();
    onTap(233, (b2t + b2b) / 2);           // NO
    ck(choiceKind == 0 && pet.speciesId == KIMERA && pet.jogressHalf == 0,
       "declining it is a plain win: still KIMERAMON");

    ck(winWild(MACHINE, false, 50) && choiceKind == 5, "the next win over one asks again");
    choiceUntil = 1;                       // let the 30 s run out
    render();
    ck(choiceKind == 0 && pet.speciesId == KIMERA, "and letting it time out declines it too");

    ck(winWild(MACHINE, false, 50) && choiceKind == 5, "(and again)");
    onTap(233, (b1t + b1b) / 2);           // JOGRESS!
    ck(choiceKind == 0 && pet.speciesId == MILLENNIUM && pet.jogressHalf == KIMERA,
       "accepting fuses into MILLENNIUMMON, KIMERAMON its own half");
    pet.badges = 0;
  }

  printf("%s\n", bad ? "FAILURES" : "all good");
  return bad ? 1 : 0;
}
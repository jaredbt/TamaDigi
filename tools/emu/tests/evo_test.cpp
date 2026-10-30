// Walks one real life: hatch a BOTAMON, age it, digivolve at each gate, and
// print the skills at every step. Answers what actually happens to skills
// across a digivolution, rather than assuming.
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
uint32_t millis() { return 0; }
void FakeESP::restart() { exit(0); }
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }
void sfxPlay(uint8_t) {}
#endif

static void show(Pet &p, const char *when) {
  printf("%-22s %-11s L%-3u :", when, TS_DEX_TBL[p.speciesId].name, p.level());
  for (int i = 0; i < MOVE_SLOTS; i++)
    printf(" [%s]", p.moves[i] ? moveName(p.moves[i]) : "-");
  printf("\n");
}

int main() {
  Pet p;
  p.dbgHatchAs(6, false);          // BOTAMON
  p.ageMinutes = 0;
  show(p, "hatched");

  auto healthy = [&] { p.fullness = p.joy = p.energy = p.hygiene = 100; };
  int offers = 0;
  for (uint32_t lvl = 1; lvl <= 100; lvl++) {
    p.ageMinutes = (lvl - 1) * MINUTES_PER_LEVEL;
    healthy();
    // trained as a player would, a session a day's worth of hours
    if (lvl % 3 == 0) { p.trainStrength(72); p.trainSpeed(36); p.playResult(36); healthy(); }
    const char *note = "";
    if (p.canEvolveNow()) { p.evolve(); note = " (evolved)"; }
    p.checkLearnGates();
    while (p.hasLearnOffer()) {          // accept every offer into the last slot
      printf("%-22s   OFFER %s -> replacing %s\n", "",
             moveName(p.learnOffer()),
             p.moves[MOVE_SLOTS-1] ? moveName(p.moves[MOVE_SLOTS-1]) : "-");
      p.acceptLearn(MOVE_SLOTS - 1);
      offers++;
    }
    if (*note) {
      char when[40];
      snprintf(when, sizeof(when), "level %u%s", lvl, note);
      show(p, when);
    }
  }
  show(p, "level 100");
  printf("%d offers over a life\n", offers);
  return 0;
}

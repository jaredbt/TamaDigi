// Headless proof that a click reaches onTap(). Replaces main_sdl.cpp's main()
// and runtime globals, then drives the REAL setup()/loop()/handleTouch() by
// poking the same touch globals the SDL event loop pokes.
//
// Build twice: with and without -DNO_IRQ, to A/B the emuFireInterrupt() fix.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "trainers.h"
#include "inventory.h"
#include <chrono>
#include <thread>
#include <string>
#include <deque>

uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;

// millis() deliberately NOT defined here: clock.cpp is linked in so this
// exercises the real emulator clock, --fast handling included.
void FakeESP::restart() { exit(0); }

// No stdin console in this harness: the serial path is not what is under test.
int FakeSerial::available() { return 0; }
String FakeSerial::readStringUntil(char) { return String(""); }

void setup();
void loop();
extern Pet pet;   // defined in the sketch
extern bool trainOpen, sackOpen, gameOpen, menuOpen, cardOpen, movePickOpen, spdOpen;
extern bool memoOpen;
extern bool bagOpen, playerOpen;
int uiMenuRowCenterY(int i);
// The training menu's geometry, read from the firmware rather than restated here.
int uiTrainRowTop(int i);
int uiTrainRowH();
int uiBagRowCenterY(int i);
extern uint8_t movePickSlot, movePickPage;
extern bool battleOpen, btlOver, btlWon;
extern Combatant btlYou, btlFoe;
extern uint8_t btlMsgCount;
void startBattle(int16_t dex, uint8_t lvl);
void startTrainerBattle(uint8_t idx, bool hard);
extern uint8_t btlMenu;
extern bool btlHard;
extern bool gymOpen, gymHard; extern uint8_t gymPage;
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
void gymRowRect(int i, int *x, int *y, int *w, int *h);
#define BTL_CELL_X(i) (69 + ((i) % 2) * 168)
#define BTL_CELL_Y(i) (286 + ((i) / 2) * 52)
uint8_t learnableList(uint16_t *out, uint8_t max);
#define MOVE_PICK_PER_PAGE 5
#define MOVE_PICK_Y(i) (76 + (i) * 58)
extern uint8_t cardPage;

// handleTouch() self-gates to 50 Hz off millis(), so real time must pass
// between polls; a tight loop() spin would be swallowed by that gate.
static void pump(int times) {
  for (int i = 0; i < times; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    loop();
  }
}

static void fire() {
#ifndef NO_IRQ
  emuFireInterrupt();
#endif
}

// One press/release at (x,y), driven exactly as the SDL event loop drives it.
static void click(int x, int y) {
  g_touchX = x; g_touchY = y; g_touchDown = true;
  fire();
  pump(5);                       // finger held: gesture starts
  g_touchDown = false;
  fire();
  pump(3);                       // finger up: gesture resolves as a tap
}

void uiButtonAt(int i, int *cx, int *cy, int *half);

int main(int argc, char **argv) {
  if (argc > 1) emuSetTimeScale((uint32_t)atoi(argv[1]));
  printf("--- time scale x%u ---\n", emuTimeScale());
  setup();
  pump(4);

  if (!pet.awaitingStarter()) { printf("FAIL: not on the starter screen\n"); return 1; }
  printf("on starter screen, awaitingStarter=1\n");

  // First boot: tap a starter and its Digi-Egg appears. Negative control first: a
  // click below every row must choose nothing, or this test proves nothing about
  // coordinate routing.
  click(233, 420);
  pump(2);
  if (!pet.awaitingStarter()) { printf("FAIL: off-row click chose a starter\n"); return 1; }
  printf("click at (233,420) off the rows: correctly ignored\n");

  click(233, 110 + 78 + 35);     // row 1: BOTAMON
  pump(2);
  if (pet.awaitingStarter()) { printf("FAIL: click did not register\n"); return 1; }
  if (pet.eggPeek() != 6) { printf("FAIL: row 1's egg is not BOTAMON\n"); return 1; }
  printf("PASS: tap reached onTap and picked a starter\n");

  // --fast must still do its job: with no finger down, game time has to run at
  // the scale. Otherwise the touch fix would have quietly neutered the flag.
  uint32_t t0 = millis();
  auto w0 = std::chrono::steady_clock::now();
  pump(20);                      // ~600 ms of real time, idle
  uint32_t gameMs = millis() - t0;
  uint64_t realMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - w0).count();
  double ratio = realMs ? (double)gameMs / (double)realMs : 0.0;
  printf("idle: %llu ms real -> %u ms game (x%.1f, want x%u)\n",
         (unsigned long long)realMs, gameMs, ratio, emuTimeScale());
  if (ratio < emuTimeScale() * 0.8 || ratio > emuTimeScale() * 1.2) {
    printf("FAIL: --fast no longer accelerates idle game time\n");
    return 1;
  }
  printf("PASS: --fast still accelerates idle game time\n");

  // ---- new UI routing: 5th icon -> training submenu -> the two live tamers,
  // and the menu's STATS row -> the stats card page. Both changed index
  // mappings inside existing handlers, which is where an off-by-one would hide.
  if (pet.isEgg()) pet.dbgHatchAs(6, false);
  pump(4);

  // The learn prompt is MODAL -- it swallows every tap until answered -- so a
  // creature with moves waiting will ignore the icons. That is correct
  // behaviour, and it started firing here as soon as digimon_data.py gained the
  // cheap early attacks: a level 1 creature now actually has things to learn.
  while (pet.hasLearnOffer()) pet.declineLearn();
  pump(2);
  int tbx, tby; uiButtonAt(3, &tbx, &tby, nullptr);   // ask, do not hardcode:
  click(tbx, tby);                       // the dumbbell, the 4th of 4
  if (!trainOpen) { printf("FAIL: train icon did not open the submenu\n"); return 1; }
  printf("PASS: 5th icon opens the training submenu\n");

  // ASKED, NOT HARDCODED. These were literal y coordinates -- 178, 242, 306 --
  // and the training menu grew a fourth row, which retuned the pitch and moved
  // every one of them onto the row below. The test then failed for the right
  // reason while pointing at the wrong thing: it was restating a layout instead
  // of reading it, which is the shape CLAUDE.md warns about and exactly why
  // uiMenuRowCenterY() a few lines down exists at all.
  {
    const int mid = uiTrainRowH() / 2;
    struct Route { int row; bool *flag; const char *what; };
    const Route route[] = {
      { 0, &sackOpen, "STRENGTH routes to the punching bag" },
      { 1, &spdOpen,  "SPEED routes to the reaction test" },
      { 2, &gameOpen, "DEFENCE routes to the ball game" },
      { 3, &memoOpen, "MEMORY routes to the memory game" },
    };
    for (const Route &r : route) {
      if (!trainOpen) click(tbx, tby);
      click(233, uiTrainRowTop(r.row) + mid);
      if (!*r.flag) {
        printf("FAIL: training row %d did not start its game\n", r.row);
        return 1;
      }
      printf("PASS: %s\n", r.what);
      *r.flag = false;
    }
  }
  trainOpen = false;

  click(233, 60);                        // name/status band opens the menu
  if (!menuOpen) { printf("FAIL: name band did not open the menu\n"); return 1; }
  click(233, uiMenuRowCenterY(0));       // asked, not hardcoded: see the note there
  if (!cardOpen || cardPage != 1) {
    printf("FAIL: STATS row -> cardOpen=%d cardPage=%d (want 1,1)\n",
           (int)cardOpen, (int)cardPage);
    return 1;
  }
  printf("PASS: menu STATS row opens the stats card page\n");

  // The row BELOW it is the bag, and it must be its own row rather than more of
  // row 0's hit area -- which is how the tap above used to land.
  cardOpen = false;
  click(233, 60);
  click(233, uiMenuRowCenterY(1));
  if (!bagOpen || cardOpen) {
    printf("FAIL: BAG row -> bagOpen=%d cardOpen=%d (want 1,0)\n",
           (int)bagOpen, (int)cardOpen);
    return 1;
  }
  printf("PASS: menu BAG row opens the bag\n");

  pet.trAtk = 0;
  pet.saveNow();
  bag.add(IT_ATKCHIP);
  uint8_t proteinRow = 0;
  while (proteinRow < bag.distinctCount() && bag.keyAt(proteinRow) != IT_ATKCHIP)
    proteinRow++;
  if (proteinRow >= bag.distinctCount()) {
    printf("FAIL: Protein did not appear in the field bag\n");
    return 1;
  }
  click(233, uiBagRowCenterY(proteinRow));
  uint8_t trainedAtk = pet.trAtk;
  Pet afterVitamin;
  afterVitamin.begin();
  if (!trainedAtk || afterVitamin.trAtk != trainedAtk) {
    printf("FAIL: Protein training did not survive reload (%u -> %u)\n",
           trainedAtk, afterVitamin.trAtk);
    return 1;
  }
  printf("PASS: training gained from a field item persists immediately\n");
  bagOpen = false;

  uint8_t oldAvatar = pet.avatar;
  playerOpen = true;
  click(233, 100);
  Pet afterAvatar;
  afterAvatar.begin();
  if (pet.avatar == oldAvatar || afterAvatar.avatar != pet.avatar) {
    printf("FAIL: avatar change did not survive reload (%u -> %u -> %u)\n",
           oldAvatar, pet.avatar, afterAvatar.avatar);
    return 1;
  }
  printf("PASS: an avatar change persists immediately\n");
  playerOpen = false;

  // Put the card back: everything below this line continues from it, and a
  // check that quietly changes the state its neighbours depend on is how a
  // suite starts failing somewhere other than where it broke.
  cardOpen = true;

  // ---- moves card page -> picker -> slot actually changes, with no duplicates
  cardPage = 2;
  uint16_t before = pet.moves[1];
  click(233, 154);                       // slot 1 == MOVE_ROW_Y(1)
  if (!movePickOpen) { printf("FAIL: tapping a move slot did not open the picker\n"); return 1; }
  printf("PASS: tapping a move slot opens the picker\n");

  click(233, 100);                       // first row of the picker
  if (movePickOpen) { printf("FAIL: picking a move left the picker open\n"); return 1; }
  if (pet.moves[1] == before) { printf("FAIL: slot 1 did not change\n"); return 1; }
  printf("PASS: picking a move replaces the slot (%s -> %s)\n",
         before ? moveName(before) : "-",
         pet.moves[1] ? moveName(pet.moves[1]) : "-");
  Pet afterMove;
  afterMove.begin();
  if (afterMove.moves[1] != pet.moves[1]) {
    printf("FAIL: the replacement move did not survive reload\n");
    return 1;
  }
  printf("PASS: a replacement move persists immediately\n");

  // the swap must never leave the same move in two slots
  bool dupe = false;
  for (int i = 0; i < MOVE_SLOTS; i++)
    for (int j = i + 1; j < MOVE_SLOTS; j++)
      if (pet.moves[i] && pet.moves[i] == pet.moves[j]) dupe = true;
  if (dupe) {
    printf("FAIL: a move ended up in two slots\n");
    for (int i = 0; i < MOVE_SLOTS; i++)
      printf("      slot %d: %s\n", i, pet.moves[i] ? moveName(pet.moves[i]) : "-");
    return 1;
  }
  printf("PASS: no duplicate move after the swap\n");

  // picking a move the pet already knows should TRADE slots, not clone it
  uint16_t s0 = pet.moves[0], s2 = pet.moves[2];
  cardPage = 2;
  click(233, 212);                       // slot 2
  uint16_t all[64];
  uint8_t n = learnableList(all, 64);
  int page = -1, row = -1;
  for (uint8_t i = 0; i < n; i++)
    if (all[i] == s0) { page = i / MOVE_PICK_PER_PAGE; row = i % MOVE_PICK_PER_PAGE; }
  if (page >= 0) {
    movePickPage = page;
    click(233, MOVE_PICK_Y(row) + 10);
    bool ok = (pet.moves[2] == s0 && pet.moves[0] == s2);
    printf("%s: picking a known move trades the two slots (0=%s 2=%s)\n",
           ok ? "PASS" : "FAIL",
           pet.moves[0] ? moveName(pet.moves[0]) : "-",
           pet.moves[2] ? moveName(pet.moves[2]) : "-");
    if (!ok) return 1;
  }
  // ---- the learn prompt: accept into a slot, and decline. DARKDRAMON has more skills
  // in its learnset than there are slots, so walking it again has to make offers.
  pet.dbgHatchAs(377, false);
  pet.ageMinutes = 40 * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  pet.learnCursor = 0;
  pet.learnQCount = 0;
  pet.checkLearnGates();
  if (!pet.hasLearnOffer()) { printf("FAIL: no learn offer was queued\n"); return 1; }
  printf("PASS: crossing gates with a full moveset queues an offer\n");

  uint16_t offered = pet.learnOffer(), was2 = pet.moves[2];
  click(233, 104 + 2 * 56 + 10);          // LEARN_ROW_Y(2)
  if (pet.moves[2] != offered) {
    printf("FAIL: accepting did not put %s in slot 2 (got %s)\n",
           moveName(offered), pet.moves[2] ? moveName(pet.moves[2]) : "-");
    return 1;
  }
  printf("PASS: accepting replaces the chosen slot (%s -> %s)\n",
         was2 ? moveName(was2) : "-", moveName(offered));

  if (pet.hasLearnOffer()) {
    uint16_t before[MOVE_SLOTS];
    for (int i = 0; i < MOVE_SLOTS; i++) before[i] = pet.moves[i];
    uint16_t skipped = pet.learnOffer();
    click(233, 334 + 20);                 // LEARN_SKIP_Y
    bool same = true;
    for (int i = 0; i < MOVE_SLOTS; i++) if (pet.moves[i] != before[i]) same = false;
    if (!same || pet.knowsMove(skipped)) {
      printf("FAIL: declining changed the moveset\n"); return 1;
    }
    printf("PASS: declining %s leaves the moveset untouched\n", moveName(skipped));
  }
  // ---- battle: drive a whole fight through the real tap handler
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.ageMinutes = 50 * MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  startBattle(9, 50);
  if (!battleOpen) { printf("FAIL: battle did not start\n"); return 1; }
  printf("PASS: BATTLE opens (%s L%u vs %s L%u)\n",
         btlYou.name, btlYou.level, btlFoe.name, btlFoe.level);

  uint16_t foeHp0 = btlFoe.hp;
  int turns = 0;
  while (battleOpen && turns < 60) {
    turns++;
    if (btlMsgCount) { click(233, 320); continue; }   // clear narration
    int slot = -1;
    for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) { slot = i; break; }
    if (slot < 0) break;
    click(BTL_CELL_X(slot) + 40, BTL_CELL_Y(slot) + 20);
  }
  printf("     after %d taps: you %u/%u, foe %u/%u, over=%d won=%d\n",
         turns, btlYou.hp, btlYou.maxHp, btlFoe.hp, btlFoe.maxHp,
         (int)btlOver, (int)btlWon);
  if (btlFoe.hp >= foeHp0) { printf("FAIL: tapping a move did no damage\n"); return 1; }
  printf("PASS: tapping a move resolves a turn and deals damage\n");
  if (!btlOver) { printf("FAIL: fight never concluded\n"); return 1; }
  printf("PASS: the fight concludes and closes\n");

  // ---- a tamer fight: 1-on-1 against the boss's strongest, and winning awards a badge
  //
  pet.ageMinutes = 100 * MINUTES_PER_LEVEL;   // strong enough to beat the first boss
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();
  bool hadBadge = pet.hasBadge(0, false);
  const Trainer &firstBoss = TRAINERS[0];
  startTrainerBattle(0, false);
  if (!battleOpen) { printf("FAIL: trainer battle did not start\n"); return 1; }
  printf("PASS: gym battle starts\n");
  if (btlFoe.dex != firstBoss.dex) {
    printf("FAIL: the first boss fought with dex %d, not %d\n", btlFoe.dex, firstBoss.dex);
    return 1;
  }
  printf("PASS: the boss fights with its one Digimon\n");

  int taps = 0;
  bool foeChanged = false;
  while (battleOpen && taps < 200) {
    taps++;
    if (btlFoe.dex != firstBoss.dex) foeChanged = true;
    if (btlMsgCount) { click(233, 320); continue; }
    int slot = -1;
    for (int i = 0; i < MOVE_SLOTS; i++) if (btlYou.moves[i]) { slot = i; break; }
    if (slot < 0) break;
    click(BTL_CELL_X(slot) + 40, BTL_CELL_Y(slot) + 20);
  }
  printf("     %d taps, won=%d\n", taps, (int)btlWon);
  if (foeChanged) { printf("FAIL: a second boss creature came out\n"); return 1; }
  printf("PASS: nobody is sent out after it: the fight is 1-on-1\n");
  if (!btlWon) { printf("FAIL: a L100 creature lost to the first boss\n"); return 1; }
  if (!pet.hasBadge(0, false) || hadBadge) { printf("FAIL: no badge awarded\n"); return 1; }
  printf("PASS: beating a leader awards its badge (%u/8)\n", pet.badgeCount(false));
  if (!pet.saveHealthy() || pet.savePending()) {
    printf("FAIL: the gym win left saving broken or pending\n");
    return 1;
  }
  Pet afterGym;
  afterGym.begin();
  if (!afterGym.hasBadge(0, false) || afterGym.trAtk != pet.trAtk ||
      afterGym.trDef != pet.trDef || afterGym.trSpe != pet.trSpe) {
    printf("FAIL: the gym badge or its training reward did not survive reload\n");
    return 1;
  }
  printf("PASS: a gym win keeps saving healthy and its badge and training survive reload\n");

  // ---- both difficulties cap your level to the boss's
  battleOpen = false;
  pet.ageMinutes = 100 * MINUTES_PER_LEVEL;   // a level 100 creature
  startTrainerBattle(0, true);                // the first boss
  const uint8_t top = firstBoss.level;
  printf("     hard vs %s: your partner is L%u (pet is L%u), boss L%u\n",
         firstBoss.name, btlYou.level, pet.level(), top);
  if (btlYou.level != top) {
    printf("FAIL: level not capped to the boss's (got %u, want %u)\n", btlYou.level, top);
    return 1;
  }
  printf("PASS: hard mode caps your level to the boss's\n");
  if (pet.level() != 100) { printf("FAIL: the stored pet was modified\n"); return 1; }
  printf("PASS: the stored creature is untouched (still L%u)\n", pet.level());
  battleOpen = false;

  // ---- de-digivolving, through the card and the confirm dialog
  {
    battleOpen = false;
    pet.dbgHatchAs(21, false);            // AGUMON
    pet.ageMinutes = 40UL * MINUTES_PER_LEVEL;
    pet.trAtk = pet.trDef = pet.trSpe = 200;
    while (pet.digiTarget() != 89) pet.declineEvolve();
    pet.evolve();
    while (pet.hasLearnOffer()) pet.declineLearn();
    if (pet.speciesId != 89) { printf("FAIL: AGUMON did not become GREYMON\n"); return 1; }
    pet.fullness = pet.joy = pet.energy = pet.hygiene = 100;
    cardOpen = true; cardPage = 3;
    pump(1);
    click(233, 386 + 22);                 // the DE-DIGIVOLVE button
    if (cardOpen) { printf("FAIL: the DE-DIGIVOLVE button did not answer\n"); return 1; }
    pump(1);
    int b1t, b1b, b2t, b2b;
    uiConfirmRects(&b1t, &b1b, &b2t, &b2b);
    click(233, (b1t + b1b) / 2);          // yes
    if (pet.speciesId != 21 || pet.deDigiCount != 1) {
      printf("FAIL: confirming did not de-digivolve (dex %d, count %u)\n", pet.speciesId, pet.deDigiCount);
      return 1;
    }
    printf("PASS: DE-DIGIVOLVE on the card asks, then takes it back to AGUMON\n");
    pump(2);
  }

  // ---- the lower-left cell is the basic attack: it spends a turn and hits
  pet.badges = 0;
  startTrainerBattle(0, false);
  btlMenu = 0;
  {
    const uint16_t foeHp0 = btlFoe.hp;
    for (int i = 0; i < 12 && btlFoe.hp == foeHp0 && battleOpen; i++) {
      click(150, 274 + 52 + 22);          // the lower-LEFT cell (RUN is right)
      while (btlMsgCount && battleOpen) click(233, 320);
    }
    if (btlFoe.hp == foeHp0) { printf("FAIL: the basic attack did nothing\n"); return 1; }
    printf("PASS: the basic attack is a cell of the battle menu, and hits\n");
  }
  battleOpen = false; btlMenu = 0;

  // ---- the boss ladder is sequential: only the next one may be entered. A row
  // starts the fight directly now; there is no team to pick.
  battleOpen = false;
  pet.badges = 0;                       // nothing beaten yet
  gymOpen = true; gymHard = false; gymPage = 0;
  click(233, 110 + 1 * 50 + 20);        // RIVAL, the second row
  if (battleOpen) { printf("FAIL: a locked leader was enterable\n"); return 1; }
  printf("PASS: a locked leader cannot be entered\n");
  gymOpen = true; gymPage = 0;
  click(233, 110 + 0 * 50 + 20);        // the first boss, the first
  if (!battleOpen) { printf("FAIL: the first leader was not enterable\n"); return 1; }
  printf("PASS: the first leader is always open, and its row starts the fight\n");
  battleOpen = false;
  pet.badges = 1;                       // the first boss beaten
  gymOpen = true; gymPage = 0;
  click(233, 110 + 1 * 50 + 20);        // RIVAL again
  if (!battleOpen) { printf("FAIL: beating one did not unlock the next\n"); return 1; }
  printf("PASS: beating a leader unlocks the next\n");
  battleOpen = false;
  // and the hard ladder is its own run
  gymOpen = true; gymHard = true; gymPage = 0;
  click(233, 110 + 1 * 50 + 20);
  if (battleOpen) { printf("FAIL: easy progress unlocked the hard ladder\n"); return 1; }
  printf("PASS: hard mode keeps its own unlock order\n");
  gymHard = false; gymOpen = false; pet.badges = 0;

  // easy caps the level too
  startTrainerBattle(0, false);
  if (btlYou.level != top) { printf("FAIL: easy mode did not cap the level\n"); return 1; }
  printf("PASS: easy mode caps the level as well\n");
  return 0;
}

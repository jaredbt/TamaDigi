#include "Arduino.h"
#include "../sketch.cpp"
#include "../../../audio.cpp"
#include "audio_stubs/runtime.h"
#include <array>
#include <tuple>
#include <deque>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

uint32_t g_seed = 0xC0FFEE;
FakeSerial Serial;
FakeESP ESP;
FakeWire Wire;
volatile int g_touchX = 0, g_touchY = 0;
volatile bool g_touchDown = false;
static std::deque<std::string> serialInput;
static unsigned serialReads = 0, loops = 0, frames = 0, activeStops = 0, voiceStops = 0;
void emuAdvanceMs(uint32_t ms);

// There were two capture cases (into the party, into the box); scanning a wild
// Digimon into storage went with the party and box, removed by design.
enum class Ending { Victory, Loss, Flee };
struct Case {
  const char *name;
  bool gym;
  Ending ending;
  bool early = false, hard = false;
};
static const Case cases[] = {
  {"gym-win", true, Ending::Victory},
  {"gym-hard-win", true, Ending::Victory, false, true},
  {"wild-win", false, Ending::Victory},
  {"gym-loss", true, Ending::Loss},
  {"wild-loss", false, Ending::Loss},
  {"gym-flee", true, Ending::Flee},
  {"wild-flee", false, Ending::Flee},
  {"gym-early", true, Ending::Victory, true},
  {"wild-early", false, Ending::Victory, true},
};
static const char *sounds[] = {"enabled", "disabled", "muted", "queued"};

void FakeESP::restart() {
  fprintf(stderr, "FAIL unexpected firmware reset during workflow\n");
  std::_Exit(85);
}
int FakeSerial::available() { return !serialInput.empty(); }
String FakeSerial::readStringUntil(char) {
  if (serialInput.empty()) return String("");
  String value(serialInput.front());
  serialInput.pop_front();
  serialReads++;
  return value;
}

static void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}

static void advance(uint32_t ms = 20) {
  bool playingNote = audioRuntime.musicNote();
  bool voices = gSyn.busy();
  uint8_t before = gMusic;
  emuAdvanceMs(ms);
  gfx->frameReady = false;
  loop();
  loops++;
  if (gfx->frameReady) frames++;
  if (playingNote && before != MUS_NONE && gMusic == MUS_NONE) activeStops++;
  if (voices && before != MUS_NONE && gMusic == MUS_NONE) voiceStops++;
  audioRuntime.advance(ms);
}

static void click(int x, int y) {
  g_touchX = x; g_touchY = y; g_touchDown = true;
  emuFireInterrupt();
  for (unsigned poll = 0; poll < 3; poll++) advance();
  g_touchDown = false;
  emuFireInterrupt();
  for (unsigned poll = 0; poll < 2; poll++) advance();
}

static void cell(int index) {
  click(BTL_CELL_X(index) + BTL_CELL_W / 2, BTL_CELL_Y(index) + BTL_CELL_H / 2);
}

// The partner, as a flat record the reboot expectations can be written in.
struct LiveMon {
  int16_t dex;
  uint16_t level, medals;
  uint8_t ivAtk, ivDef, ivSpe, ivHp, trAtk, trDef, trSpe, variant;
  uint8_t fullness, joy, energy, hygiene, poops, weight, bond, berryKnown;
  uint8_t careMistakes, lastLearnLevel;
  uint32_t ageMinutes;
  char nick[12];
  uint8_t moves[MOVE_SLOTS];
};

static LiveMon liveMon() {
  LiveMon m{};
  m.dex = pet.speciesId; m.level = pet.level(); m.medals = pet.medals;
  m.ivAtk = pet.ivAtk; m.ivDef = pet.ivDef; m.ivSpe = pet.ivSpe; m.ivHp = pet.ivHp;
  m.trAtk = pet.trAtk; m.trDef = pet.trDef; m.trSpe = pet.trSpe; m.variant = pet.variant;
  m.fullness = pet.fullness; m.joy = pet.joy; m.energy = pet.energy; m.hygiene = pet.hygiene;
  m.poops = pet.poops; m.weight = pet.weight; m.bond = pet.bond; m.berryKnown = pet.berryKnown;
  m.careMistakes = pet.careMistakes; m.lastLearnLevel = pet.lastLearnLevel;
  m.ageMinutes = pet.ageMinutes;
  memcpy(m.nick, pet.nick, sizeof(m.nick));
  memcpy(m.moves, pet.moves, sizeof(m.moves));
  return m;
}

struct Collection {
  LiveMon live;
  std::array<uint16_t, GYM_REGIONS> easy, hard;
  std::array<uint8_t, sizeof(pet.dexReg)> dex, variant;
  std::array<uint8_t, ITEM_COUNT> items;
  char trainer[sizeof(pet.trainerName)] = {};
  uint8_t avatar = 0, region = 0;
  uint16_t medals = 0, streak = 0, bestStreak = 0;
  bool sound = true;
  uint8_t volume = 7;
};

static Collection collection() {
  Collection result{};
  result.live = liveMon();
  for (uint8_t region = 0; region < GYM_REGIONS; region++) {
    result.easy[region] = pet.badgeMask(region, false);
    result.hard[region] = pet.badgeMask(region, true);
  }
  std::copy(std::begin(pet.dexReg), std::end(pet.dexReg), result.dex.begin());
  std::copy(std::begin(pet.dexVariantReg), std::end(pet.dexVariantReg), result.variant.begin());
  memcpy(result.trainer, pet.trainerName, sizeof(result.trainer));
  result.avatar = pet.avatar; result.region = pet.region;
  result.medals = pet.totalMedals; result.streak = pet.streak; result.bestStreak = pet.bestStreak;
  for (ItemKey key = 1; key < ITEM_COUNT; key++) result.items[key] = bag.count(key);
  result.sound = audioEnabled(); result.volume = audioVolume();
  return result;
}

static bool sameMon(const LiveMon &left, const LiveMon &right) {
  return std::tie(left.dex, left.level, left.medals, left.ivAtk, left.ivDef, left.ivSpe,
                  left.ivHp, left.trAtk, left.trDef, left.trSpe, left.variant,
                  left.fullness, left.joy, left.energy, left.hygiene, left.poops, left.weight,
                  left.bond, left.berryKnown, left.careMistakes,
                  left.lastLearnLevel, left.ageMinutes) ==
         std::tie(right.dex, right.level, right.medals, right.ivAtk, right.ivDef, right.ivSpe,
                  right.ivHp, right.trAtk, right.trDef, right.trSpe, right.variant,
                  right.fullness, right.joy, right.energy, right.hygiene, right.poops, right.weight,
                  right.bond, right.berryKnown, right.careMistakes,
                  right.lastLearnLevel, right.ageMinutes) &&
         !memcmp(left.nick, right.nick, sizeof(left.nick)) &&
         !memcmp(left.moves, right.moves, sizeof(left.moves));
}

static void compareCollection(const Collection &expected, bool live) {
  Collection actual = collection();
  if (live) require(sameMon(actual.live, expected.live), "persisted Digimon or training differs after reboot");
  require(actual.easy == expected.easy && actual.hard == expected.hard, "persisted badges differ");
  require(actual.dex == expected.dex && actual.variant == expected.variant, "existing Digidex differs");
  require(actual.items == expected.items, "inventory rewards differ after battle/reboot");
  require(actual.sound == expected.sound && actual.volume == expected.volume, "audio settings differ after reboot");
  require(!memcmp(actual.trainer, expected.trainer, sizeof(actual.trainer)) &&
          actual.avatar == expected.avatar && actual.region == expected.region &&
          actual.medals == expected.medals && actual.streak == expected.streak &&
          actual.bestStreak == expected.bestStreak, "existing player data differs");
}

static void fixture(const Case &test) {
  for (size_t index = 0; index < sizeof(pet.dexReg); index++) {
    pet.dexReg[index] = (uint8_t)(0x55 + index);
    pet.dexVariantReg[index] = (uint8_t)(0xAA - index);
  }
  bool losing = test.ending == Ending::Loss;
  pet.dbgHatchAs(losing ? 1 : 9, true);
  pet.ageMinutes = losing ? 0 : 99 * MINUTES_PER_LEVEL;
  pet.ivAtk = pet.ivDef = pet.ivSpe = pet.ivHp = losing ? 0 : 31;
  pet.trAtk = losing ? 0 : 20; pet.trDef = losing ? 0 : 22; pet.trSpe = losing ? 0 : 24;
  pet.relearnFromLevel();
  if (losing) {
    uint8_t growl = 0;
    for (uint8_t move = 1; move < MOVE_COUNT; move++)
      if (!strcmp(MOVE_TBL[move].name, "GROWL")) growl = move;
    require(growl != 0, "loss fixture needs the real Growl move");
    memset(pet.moves, 0, sizeof(pet.moves));
    pet.moves[0] = growl;
  }
  while (pet.hasLearnOffer()) pet.declineLearn();
  pet.lastLearnLevel = pet.level();
  pet.medals = 0xFF; pet.totalMedals = 19;
  pet.streak = 3; pet.bestStreak = 7;
  pet.avatar = 3; pet.region = 0;
  strcpy(pet.nick, "E2E PET"); strcpy(pet.trainerName, "E2E PLAYER");
  pet.badges = 1u << 7; pet.badgesHard = 1u << 5;
  for (uint8_t region = 1; region < GYM_REGIONS; region++) {
    pet.badgesX[region - 1] = (uint16_t)(1u << region);
    pet.badgesHardX[region - 1] = (uint16_t)(3u << region);
  }
  pet.saveNow();
  advance();
  require(pet.saveHealthy() && !pet.savePending(), "fixture saving is not healthy");
  require(uiCurrentScreen() == SCR_MAIN, "fixture is not on main");
}

static void waitMusic(uint8_t music) {
  require(gMusic == music, "battle handler did not request its real music");
  if (!audioEnabled()) { advance(80); return; }
  for (unsigned step = 0; step < 500; step++) {
    if (gMusic == music && audioRuntime.musicNote()) return;
    advance();
  }
  throw std::runtime_error("missing milestone: real music never started an active note");
}

static void battleWorkflow(const Case &test, Collection &preserved, const char *sound) {
  onSwipe(-1); onSwipe(-1);
  require(gymOpen && gymPick, "gym chooser was not reached through the swipe handler");
  click(uiRegionRowX(), uiRegionRowCenterY(0));
  require(gymOpen && !gymPick && gymRegion == 0, "Ver.1 selection did not reach the gym ladder");
  if (test.hard) click(233, GYMDIF_Y + GYMDIF_H / 2);
  int x, y, width, height;
  gymRowRect(0, &x, &y, &width, &height);
  // One partner, so a leader's row starts the fight directly (it used to open a
  // team picker); a wild fight is reached by swiping from the ladder to Explore.
  if (test.gym) {
    click(x + width / 2, y + height / 2);
  } else {
    onSwipe(1);
    require(exploreOpen, "Explore was not reached through the swipe handler");
    if (test.ending == Ending::Loss) click(233, EXPLORE_DIF_Y + EXPLORE_DIF_H / 2);
    click(EXPLORE_BTN_X + EXPLORE_BTN_W / 2, EXPLORE_BTN_Y + EXPLORE_BTN_H / 2);
  }
  require(battleOpen && btlYou.dex == pet.speciesId && btlWild == !test.gym,
          "missing milestone: real battle did not start");
  waitMusic(MUS_BATTLE);
  printf("MILESTONE battle-started %s sound=%s music-writes=%zu\n", test.name, sound, audioRuntime.battleWrites);
  unsigned attacks = 0;
  if (test.ending == Ending::Flee) {
    for (unsigned tap = 0; battleOpen && !btlOver && tap < 40; tap++) {
      if (btlMsgCount) click(233, 320);
      else cell(3);
    }
    if (!test.gym)
      require(btlOver && !btlWon && !strcmp(btlMsg[0], T(S_GOT_AWAY)), "player flee did not reach the real escape result");
  } else {
    for (unsigned tap = 0; tap < 300 && !btlOver; tap++) {
      if (btlMsgCount) { click(233, 320); continue; }
      if (btlMenu == 0) { cell(0); continue; }
      uint8_t move = test.ending == Ending::Loss ? btlYou.moves[0] : aiChooseMove(btlYou, btlFoe, true);
      int slot = 0;
      while (slot < MOVE_SLOTS && btlYou.moves[slot] != move) slot++;
      require(slot < MOVE_SLOTS, "battle has no usable move");
      cell(slot);
      attacks++;
    }
    require(attacks > 0 && btlOver, "vacuous battle coverage: no real turn reached its result");
    if (test.ending == Ending::Victory)
      require(btlWon && btlFoe.fainted(), "missing milestone: real battle victory");
    else
      require(!btlWon && btlYou.fainted(), "missing milestone: real battle loss rather than foe fleeing");
  }
  if (test.gym && test.ending == Ending::Victory) {
    require(pet.hasBadge(0, 0, test.hard) && btlTrainGain > 0, "gym badge or training was not awarded");
    (test.hard ? preserved.hard : preserved.easy)[0] |= 1;
  }
  if (!test.gym && btlWon) {
    require(wildDropN > 0, "wild victory did not award its real drops");
    for (uint8_t index = 0; index < wildDropN; index++) preserved.items[wildDrops[index]]++;
  }
  require(pet.saveHealthy() && !pet.savePending(), "battle result left saving broken or pending");
  if (battleOpen && btlWon && !test.early) waitMusic(MUS_VICTORY);
  size_t effectsBefore = audioRuntime.received;
  if (!strcmp(sound, "queued") && battleOpen) {
    for (unsigned effect = 0; effect < audioRuntime.capacity + 4; effect++) sfxPlay(SFX_HIT);
    require(audioRuntime.requests.size() == audioRuntime.capacity && audioRuntime.dropped >= 4,
            "vacuous queued-effect coverage: bounded queue never filled");
  }
  if (test.early) require(btlMsgCount > 0, "early dismissal did not encounter pending narration");
  for (unsigned tap = 0; tap < 40 && battleOpen; tap++) {
    bool finalTap = btlWinUntil || (btlOver && btlMsgCount == 1);
    bool interrupt = finalTap && btlWon && !test.early && audioEnabled() && strcmp(sound, "queued");
    if (interrupt) {
      waitMusic(MUS_VICTORY);
      require(audioRuntime.musicNote(), "final dismissal has no active music note");
      onTap(233, 320);
      require(gMusic == MUS_NONE, "final exit handler did not stop the active music");
      activeStops++; voiceStops++;
      advance();
    } else click(233, 320);
  }
  require(!battleOpen && uiCurrentScreen() == SCR_MAIN, "result dismissal did not return to main");
  if (audioEnabled() && test.ending == Ending::Victory && !test.early && strcmp(sound, "queued"))
    require(activeStops > 0, "vacuous audio coverage: no active music note was interrupted by an exit handler");
  for (unsigned step = 0; step < 200 && !audioRuntime.idle(); step++) advance();
  if (!strcmp(sound, "queued") && test.ending != Ending::Flee)
    require(audioRuntime.received >= effectsBefore + audioRuntime.capacity, "queued effects were not executed after exit");
  printf("MILESTONE results-dismissed music-stops=%u voice-stops=%u effects=%zu\n",
         activeStops, voiceStops, audioRuntime.received);
}

static void idle() {
  uint32_t began = millis();
  unsigned loopsBefore = loops, framesBefore = frames;
  size_t waitsBefore = audioRuntime.waits;
  for (unsigned step = 0; step < 1500; step++) {
    advance();
    require(pet.saveHealthy(), "save failure during post-battle idle");
    require(!battleOpen && uiCurrentScreen() == SCR_MAIN, "main stopped responding during idle");
    if (step % 250 == 249) {
      unsigned readsBefore = serialReads;
      serialInput.push_back("STATS");
      advance();
      require(serialReads == readsBefore + 1, "serial handler stopped responding during idle");
      click(233, 60);
      require(uiCurrentScreen() == SCR_MENU, "touch handler stopped responding during idle");
      onSwipe(1);
      require(uiCurrentScreen() == SCR_MAIN, "menu exit did not return to main during idle");
    }
  }
  for (unsigned step = 0; step < 100 && !audioRuntime.idle(); step++) advance();
  require(millis() - began >= 30000 && loops - loopsBefore >= 1500 && frames - framesBefore >= 300,
          "missing milestone: 30 seconds of meaningful firmware execution");
  require(audioRuntime.idle() && audioRuntime.waits - waitsBefore >= 700,
          "audio task did not keep yielding throughout post-battle idle");
  require(pet.saveHealthy() && !pet.savePending(), "idle ended with saving broken or pending");
  printf("MILESTONE idle-30s loops=%u frames=%u waits=%zu\n",
         loops - loopsBefore, frames - framesBefore, audioRuntime.waits - waitsBefore);
}

static void writeExpected(const Collection &expected) {
  FILE *stream = fopen("expected.bin", "wb");
  require(stream != nullptr, "cannot write reboot expectations");
  bool wrote = fwrite(&expected, sizeof(expected), 1, stream) == 1;
  require(fclose(stream) == 0 && wrote, "reboot expectations were truncated");
}

static int scenario(const char *program, const Case &test, const char *sound) {
  {
    Preferences settings;
    settings.begin("tamadigi", false);
    settings.putBool("snd", strcmp(sound, "disabled") != 0);
    settings.putUChar("vol", !strcmp(sound, "muted") ? 0 : 7);
  }
  setup();
  require(gReady && audioRuntime.taskStarts == 1 && audioRuntime.capacity == 8,
          "vacuous audio coverage: real audio initializer/task/queue unavailable");
    fixture(test);
  Collection preserved = collection();
    battleWorkflow(test, preserved, sound);
  compareCollection(preserved, false);
  idle();
  Collection expected = collection();
    unsigned training = expected.live.trAtk + expected.live.trDef + expected.live.trSpe;
    unsigned before = preserved.live.trAtk + preserved.live.trDef + preserved.live.trSpe;
    require(test.gym && test.ending == Ending::Victory ? training > before : training == before,
      "training reward changed unexpectedly before reboot");
    require(expected.live.dex == preserved.live.dex && expected.live.variant == preserved.live.variant &&
      expected.live.ivAtk == preserved.live.ivAtk && expected.live.ivDef == preserved.live.ivDef &&
      expected.live.ivSpe == preserved.live.ivSpe && expected.live.ivHp == preserved.live.ivHp &&
      !memcmp(expected.live.nick, preserved.live.nick, sizeof(expected.live.nick)) &&
      !memcmp(expected.live.moves, preserved.live.moves, sizeof(expected.live.moves)),
      "existing Digimon identity changed during battle/idle");
    if (!strcmp(sound, "disabled")) require(audioRuntime.writes == 0, "disabled audio wrote PCM");
    else {
      require(audioRuntime.battleWrites > 0 && audioRuntime.samples > 0, "vacuous audio coverage: battle never produced PCM");
      require(!strcmp(sound, "muted") ? audioRuntime.audible == 0 : audioRuntime.audible > 0,
        "muted/enabled PCM did not match the actual sound setting");
    }
  writeExpected(expected);
  nvsSave("workflow.nvs");
  audioRuntime.stop();
  printf("MILESTONE reboot-requested\n");
  fflush(stdout);
  execl(program, program, "--reload", test.name, sound, (char *)nullptr);
  throw std::runtime_error("could not reboot workflow process");
}

static int reload() {
  Collection expected{};
  FILE *stream = fopen("expected.bin", "rb");
  require(stream != nullptr, "missing reboot expectations");
  bool read = fread(&expected, sizeof(expected), 1, stream) == 1;
  require(fclose(stream) == 0 && read, "truncated reboot expectations");
  nvsLoad("workflow.nvs");
  require(!nvs().empty(), "missing persisted NVS image");
  setup();
  require(pet.saveHealthy() && !pet.savePending(), "saving unhealthy after reboot");
  compareCollection(expected, true);
  FILE *done = fopen("workflow.done", "wb");
  require(done != nullptr, "cannot record completion milestone");
  require(fputs("reloaded\n", done) >= 0 && fclose(done) == 0, "cannot commit completion milestone");
  printf("MILESTONE reboot-reloaded rewards-and-collection-preserved\n");
  return 0;
}

static void runIsolated(const char *program, const Case &test, const char *sound) {
    char directory[] = "/tmp/tamadigi-workflow-XXXXXX";
    require(mkdtemp(directory) != nullptr, "cannot create disposable save directory");
    pid_t child = fork();
    require(child >= 0, "cannot start isolated workflow");
    if (!child) {
      alarm(45);
      if (chdir(directory)) std::_Exit(87);
      execl(program, program, "--run", test.name, sound, (char *)nullptr);
      std::_Exit(87);
    }
    int outcome = 0;
    require(waitpid(child, &outcome, 0) == child, "workflow child disappeared");
    std::string base(directory);
    FILE *done = fopen((base + "/workflow.done").c_str(), "rb");
    bool completed = done != nullptr;
    if (done) fclose(done);
    for (const char *file : {"expected.bin", "workflow.nvs", "workflow.done"})
      unlink((base + "/" + file).c_str());
    rmdir(directory);
    require(WIFEXITED(outcome) && WEXITSTATUS(outcome) == 0 && completed,
            "isolated workflow failed, timed out, reset, or missed its reboot milestone");
    printf("PASS workflow %s sound=%s battle -> exit -> idle -> cold reboot\n", test.name, sound);
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  try {
    const Case *selected = nullptr;
    const char *sound = nullptr;
    if (argc == 4) {
      for (const Case &test : cases) if (!strcmp(test.name, argv[2])) selected = &test;
      for (const char *mode : sounds) if (!strcmp(mode, argv[3])) sound = mode;
      require(selected && sound, "unknown mandatory workflow case or sound mode");
      if (!strcmp(argv[1], "--reload")) return reload();
      if (!strcmp(argv[1], "--run")) return scenario(argv[0], *selected, sound);
      require(!strcmp(argv[1], "--case"), "usage: workflow_test [--case NAME SOUND]");
    } else require(argc == 1, "usage: workflow_test [--case NAME SOUND]");
    char program[PATH_MAX];
    require(realpath(argv[0], program) != nullptr, "cannot resolve workflow executable");
    unsigned completed = 0;
    for (const Case &test : cases)
      for (const char *mode : sounds) {
        if (selected && (&test != selected || strcmp(mode, sound))) continue;
        runIsolated(program, test, mode);
        completed++;
      }
    require(completed == (selected ? 1 : std::size(cases) * std::size(sounds)), "mandatory workflow matrix is incomplete");
    printf("PASS mandatory workflows complete: %u\n", completed);
    return 0;
  } catch (const std::exception &failure) {
    fprintf(stderr, "FAIL workflow: %s\n", failure.what());
    return 1;
  }
}
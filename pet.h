#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "roster.h"
#include "trainers.h"

#define MOVE_SLOTS 4    // four skill slots, as Time Stranger has
// The largest the creature and player checkpoint blobs may be, read buffers
// included. Here rather than in pet.cpp because save.cpp sizes the backup's
// per-value ceiling from them; see the notes beside their uses in pet.cpp.
#define PET_CKPT_CAP 192
#define PLAYER_CKPT_CAP 512

// 1 tick = 1 minute of play. Lower this value to test faster
// (e.g. 5000UL = the stats drop 12x faster).
#define PET_TICK_MS 60000UL
// Minutes of play per level. At 60, AGUMON evolves after ~16 h
// of play with perfect care. Drop to 1 to see evolutions immediately.
#define MINUTES_PER_LEVEL 60
#define MAX_LEVEL 100              // reached at 4d 3h; see level()
#define EAT_ANIM_MS 2500UL
#define HEART_MS 1500UL
#define EVOLVE_ANIM_MS 5200UL              // evolution animation (longer = more epic)
#define CEREMONY_MS 10000UL                // on-screen duration of the farewell
#define FAREWELL_AGE_MIN (3UL * 24 * 60)   // says goodbye after 3 days of play (in its final form)
// Retiring a creature BEFORE it has earned its farewell costs the NEXT one a
// day's worth of digivolution. Derived from MINUTES_PER_LEVEL rather than written
// as 24, so it stays "a day" if the level rate is ever retuned.
#define EVO_PENALTY_LEVELS ((uint8_t)((24UL * 60) / MINUTES_PER_LEVEL))
#define RUNAWAY_TICKS 60                   // runs away after 1 h with EVERYTHING at zero
// Night, by the RTC: midnight to 06:00. Auto-sleep needs BOTH: the screen off
// AND this window.
// The screen alone would pause the game every time you put the device in a
// pocket -- the creature is supposed to get hungry during the day. The hour
// alone would send it to bed while you were playing. Set the clock in SETTINGS
// or over the console with RTCSET; an unset board reads months out and simply
// never auto-sleeps, which fails in the safe direction (it still drains, and
// the light button still works by hand).
#define NIGHT_START 0
#define NIGHT_END 6
enum : uint8_t { SLEEP_NONE = 0, SLEEP_AUTO, SLEEP_PLAYER };
#define DEF_TRAIN_TICKS 60                 // minutes of well-being per +1 DEF
#define TRAIN_MAX 255                      // every stat, every Digimon: a bad IV trains longer
// A de-digivolution multiplies the partner's HP by a further 10%, up to x2.0.
// Time Stranger's requirements assume its level-reset loops, and this is what stands
// in for them: it is how the HP minimums above a form's level-99 HP are reached.
#define DEDIGI_HP_STEP_PCT 10
#define DEDIGI_HP_MAX_PCT 200
#define DEDIGI_COUNT_MAX ((DEDIGI_HP_MAX_PCT - 100) / DEDIGI_HP_STEP_PCT)
// PHASE 2 REPLACES THIS. Agent Rank comes from the boss ladder (1 + area badges, 10
// for the champion). Until that lands the rank check passes for every route: a
// real rank of 1 would shut every route into Ultimate and later, which the ladder
// has no way yet to open. See Pet::agentRank().
#define AGENT_RANK_STUB 255
#define EVO_OPTIONS_MAX 32                 // more than any form has open at once

// ---------------------------------------------------------------------------
// WHAT A CLOCK READING IS WORTH.
//
// Offline progression multiplies whatever the RTC says by a whole life's worth
// of decay, so the reading is the single most load-bearing number in the save --
// and it comes off a physical part with its own battery that can read zero, come
// up unset, or be dragged anywhere by a host command. The old code trusted it
// three ways over: it adopted a ZERO as the new reference (destroying the only
// record of when the device was last seen, and turning the next good reading
// into a decades-long jump), it rebased silently onto a BACKWARD jump, and it
// CLAMPED a forward jump of any size to two weeks and then applied all of it --
// aging a creature fourteen days it never lived off a garbage reading, which can
// cross the farewell threshold and the level cap in one boot.
//
// So a reading is judged ONCE, here, and every caller asks. Nothing is ever
// clamped away: a rejected or rebased reading costs the player no progress at
// all, it only declines to invent any.
enum ClockVerdict : uint8_t {
  CLK_REJECT = 0,  // not a time at all: keep the reference we already had
  CLK_REBASE,      // a real time, but not a plausible amount of elapsed life
  CLK_APPLY,       // trustworthy: apply *minsOut minutes of offline progression
};
// Nothing before this is a real time: it is the epoch a virgin RTC is seeded
// with, so a reading below it predates the firmware that could have written it.
// The sketch seeds from this same constant rather than restating the number.
#define CLOCK_EPOCH_FLOOR 1767225600UL
// The most offline time that is worth applying. A genuine two-week absence is
// plausible; anything longer is a clock fault, not a holiday.
#define CLOCK_MAX_OFFLINE_MINS (14UL * 24 * 60)
// A reading may sit slightly behind the stored one without being a fault: the
// RTC and millis() drift apart, and the 30 s stamp in loop() can land either
// side of a second. Beyond this the clock genuinely went backwards.
#define CLOCK_BACK_SLACK_SECS 120UL

// THE single answer. `seen` is the stored reference (0 = never set), `now` the
// fresh reading. Writes the minutes to apply into *minsOut, which is 0 for
// anything but CLK_APPLY.
ClockVerdict clockVerdict(uint32_t seen, uint32_t now, uint32_t *minsOut);

// end-of-cycle ceremonies
enum : uint8_t { CER_NONE = 0, CER_FAREWELL, CER_RUNAWAY, CER_RELEASE };

enum PetMood : uint8_t { MOOD_HAPPY, MOOD_SAD, MOOD_EATING, MOOD_SLEEPING };

// Derived from the IVs by Pet::personality(); see the essay there. The first four
// are IN THE SAME ORDER as the IVs they read (ATK, DEF, SPE, HP), which is what
// lets that function return the winning index directly instead of mapping it.
enum : uint8_t {
  PERS_BOLD = 0,   // strongest attack
  PERS_STURDY,     // strongest defence
  PERS_BRISK,      // fastest
  PERS_HARDY,      // toughest
  PERS_EAGER,      // no standout stat, but good all round
  PERS_CALM,       // no standout stat, and modest -- also what an egg reads as
};

// the individual's medals (bitmask)
enum : uint16_t {
  MED_LV10 = 1 << 0, MED_LV25 = 1 << 1, MED_LV50 = 1 << 2,
  MED_BERRY = 1 << 3, MED_STREAK7 = 1 << 4, MED_BOND = 1 << 5,
  MED_FINAL = 1 << 6, MED_FIT = 1 << 7,
};
#define MED_COUNT 8

// THE stat formula: a Digimon's linear growth to this level, the one scale
// constant, plus training. Pet's own stat getters and combatantFromPet() (which may
// cap the level) both call it, so there is one copy of the maths. `stat` is
// TS_STAT_*. HP has no training: petCalcHp() is its form, with the de-digivolve boost.
uint16_t petCalcStat(int16_t dex, uint8_t stat, uint8_t lvl, uint8_t tr);
uint16_t petCalcHp(int16_t dex, uint8_t lvl, uint8_t deDigiCount);

// What a training session pays once the IV has scaled it: 70% at IV 0, 130% at IV 31,
// so an average IV (~15) trains as the raw figure says. Mini-games only. IVs no
// longer add to a stat directly; they decide how fast it is trained.
uint8_t trainGainFor(uint8_t raw, uint8_t iv);

// ---------------------------------------------------------------------------
// Is the sprite pack on the microSD?
//
// THE DEFAULT IS TRUE, which is doing two jobs: a board with no SD at all keeps
// today's behaviour rather than presenting an empty game, and pet.cpp stays free
// of any SD dependency. That second part is not tidiness -- pet.cpp is compiled
// into every test binary and none of them link sdmon.cpp, so a direct call would
// mean stubbing it in all of them. The firmware narrows this ONCE, after the card
// mounts. Same shape as link.cpp taking a transport pointer instead of calling
// ESP-NOW across a layer, and for the same reason: the tests can drive it.
extern bool gArtPack;

// Three drawable forms, spread across the roster, for checking the pack is whole.
// 0 for an invalid slot.
int16_t artProbe(uint8_t slot);

// Can this Digimon be put on the panel at all: it has art, and the pack is on the
// card. THE single answer, asked by the egg pool, every digivolution and the wild.
bool speciesShowable(int16_t d);

// The In-Training I forms that can hatch, in dex order. Writes up to `max` and
// returns how many there are. The first-boot starters and the egg pool are this list.
uint8_t starterForms(int16_t *out, uint8_t max);

class Pet {
public:
  Pet() = default;
  Pet(const Pet &) = delete;
  Pet &operator=(const Pet &) = delete;

  // Stats 0..100
  uint8_t fullness = 80;  // food
  uint8_t joy = 80;       // happiness
  uint8_t energy = 80;    // energy
  uint8_t hygiene = 100;  // cleanliness
  uint8_t poops = 0;      // poops on screen (max 3)
  uint8_t weight = 0;     // 0-100: treats fatten, the mini-game burns
  // IVs (individual values 0-31): rolled at hatching and never change. They
  // scale what each training session pays (trainGainFor()) and name the
  // personality; they do not add to a stat. A mediocre individual gets as far as
  // any other, it just takes more sessions.
  uint8_t ivAtk = 16, ivDef = 16, ivSpe = 16, ivHp = 16;
  uint8_t trAtk = 0, trDef = 0, trSpe = 0;
  bool berryKnown = false;  // has already discovered its favourite berry
  bool variant = false;       // rare colour variant (rolled in the egg)
  uint32_t ageMinutes = 0;
  int16_t speciesId = -1;      // Digidex number (1..DEX_COUNT), -1 = egg
  int16_t prevSpeciesId = -1;  // for the evolution animation
  uint8_t careMistakes = 0;   // care mistakes this stage
  // De-digivolutions this partner has made, up to DEDIGI_COUNT_MAX: the HP boost.
  // Kept through later evolutions, reset with a new partner.
  uint8_t deDigiCount = 0;
  // The level a de-digivolved form must reach before it may evolve again: it waits
  // out its stage. 0 once it has evolved.
  uint8_t evoHold = 0;
  uint32_t formAgeMin = 0;    // ageMinutes when it became this form: the skill clock
  bool sleeping = false;
  uint32_t lastSeenEpoch = 0;   // last RTC time seen (for offline progression)
  uint8_t ceremony = CER_NONE;  // farewell/runaway/release in progress
  uint8_t lastEnd = CER_NONE;   // how the previous one ended (affects the egg)
  // Digidex bitmaps, one bit per Digimon, in dex-id order.
  uint8_t dexReg[(DEX_COUNT + 7) / 8] = { 0 };       // raised
  uint8_t dexVariantReg[(DEX_COUNT + 7) / 8] = { 0 };  // raised in the variant version
  // daily care streak (the player's: persists across raisings)
  uint16_t streak = 0, bestStreak = 0;
  uint32_t lastCareDay = 0;
  // bond (the creature's: rises slowly with care, resets when another hatches)
  uint8_t bond = 0;
  char nick[12] = "";    // nickname (empty = species name)
  // medals: the individual's + a running count across all raisings
  uint16_t medals = 0, totalMedals = 0;
  uint16_t newMedal = 0;   // just earned, to celebrate
  uint16_t lastMilestone = 0;  // streak milestone already celebrated
  uint16_t gameHi = 0;     // mini-game record (the player's)
  uint16_t strHi = 0;      // bag hits record

  void begin();                 // loads the state from NVS (or creates the first egg)
  void update(uint32_t nowMs);  // call on every loop()

  // Actions (touch buttons)
  void feed();              // baya roja (compatibilidad)
  void feedBerry(uint8_t color);  // 0 red, 1 blue, 2 green
  void feedCandy();
  bool lovesBerry(uint8_t color) const {
    return !isEgg() && (speciesId % 3) == color;  // hidden per-species preference
  }

  // ---------------------------------------------------------------------------
  // PERSONALITY: flavour, DERIVED, never stored.
  //
  // No new save field, and none is needed: the IVs are rolled once at hatch and
  // never change again, and they are already persisted -- so anything computed
  // from them is stable across reboots for free. Same trick as lovesBerry()
  // above, which hides a per-Digimon taste inside speciesId.
  //
  // Deliberately NOT derived from speciesId: that changes when the creature
  // digivolves, and a companion whose temperament resets the moment it grows up is
  // worse than having none at all.
  //
  // And deliberately NOT a hash of the IVs. A hash is stable but arbitrary --
  // it tells the player nothing and cannot be wrong, which also means it cannot
  // be interesting. This READS THE STATS: the standout IV names the trait, so a
  // BRISK creature really is the fast one, and a player who checks its speed
  // finds the description was true. A creature with no standout stat is
  // described by its overall quality instead, which is the honest reading of a
  // spread with nothing to single out.
  static const uint8_t PERSONALITY_COUNT = 6;
  uint8_t personality() const {
    if (isEgg()) return PERS_CALM;
    const uint8_t v[4] = { ivAtk, ivDef, ivSpe, ivHp };
    uint8_t hi = 0, lo = 0;
    for (uint8_t i = 1; i < 4; i++) {
      if (v[i] > v[hi]) hi = i;      // ties keep the FIRST, so ATK/DEF/SPE/HP is
      if (v[i] < v[lo]) lo = i;      // the tie-break order and it is deterministic
    }
    if ((uint8_t)(v[hi] - v[lo]) <= 4) {
      const uint16_t sum = (uint16_t)ivAtk + ivDef + ivSpe + ivHp;
      return sum >= 4 * 20 ? PERS_EAGER : PERS_CALM;
    }
    // PERS_BOLD/STURDY/BRISK/HARDY are in the same order as v[] above, so the
    // winning index IS the trait. One correspondence rather than a switch that
    // could disagree with the array.
    return hi;
  }
  // The ball game: happiness AND defence training. Returns the DEF gained.
  uint8_t playResult(uint8_t score);
  uint8_t trainStrength(uint16_t hits);  // training bag (trains STR)
  // Reaction test: its own tamer, so the ball game can go back to being purely
  // about joy instead of doubling as a stat grind.
  uint8_t trainSpeed(uint16_t hits);
  // What beating a boss is worth. Random WHICH stat, but only among the
  // ones with room left -- a random grant that landed on an already-capped stat
  // would silently evaporate, which reads as a bug rather than as luck. Writes
  // the stat into `which` (0 ATK, 1 DEF, 2 SPE) and returns what was actually
  // gained; 0 means every stat is at its ceiling. Costs nothing: the fight
  // already spent the energy, and that is what rate-limits rematching.
  uint8_t rewardTraining(uint8_t amount, uint8_t &which);
  uint16_t spdHi = 0;    // best reaction-test score
  // Longest sequence repeated back in the memory game. Player-wide like the other
  // three records, so it outlives the creature that set it.
  uint16_t memoHi = 0;
  // The memory game's reward. Deliberately joy and bond rather than a stat: all
  // three trainable stats already have a game (the bag trains ATK, the reaction
  // test SPE, the ball DEF), so a fourth would be another grind rather than
  // another thing to do. Returns the joy actually gained.
  uint8_t playMemo(uint16_t best);

  // battle stats: Time Stranger growth at this level, scaled, plus training. Each
  // is petCalcStat() at level(); combatantFromPet() asks the same function at a
  // capped level, so the formula has one home.
  uint16_t atkStat() const;
  uint16_t defStat() const;
  uint16_t speStat() const;
  uint16_t vitStat() const;  // HP: age only, with the de-digivolve boost
  // Magic attack reuses trAtk and magic defence reuses trDef: ATK training also
  // raises magic attack, DEF training also magic defence.
  uint16_t spaStat() const;
  uint16_t spdStat() const;

  // The four known skills (ids into TS_SKILL_TBL; 0 = empty slot). Skills carry
  // over on digivolving and de-digivolving, specials included: any form can use
  // any skill in its slots.
  uint16_t moves[MOVE_SLOTS] = { 0, 0, 0, 0 };
  uint8_t moveCount() const;
  bool knowsMove(uint16_t mv) const;
  // Hours since it became this form, the clock attachment skills unlock on.
  uint8_t formHours() const;
  // A default set for this form at this age, with no offers: its specials, then
  // the newest attachment skills it has unlocked. For a Digimon that did not
  // live through the learning, an opponent or a console-made partner.
  void relearnFromLevel();
  // Skills this form offers and has unlocked that are not in a slot. Returns how
  // many were written into out (at most max).
  uint8_t pendingLearnables(uint16_t *out, uint8_t max) const;

  // Player-wide, like the streak and the Digidex: badges outlive the creature
  // that earned them, so newEgg() must never clear this.
  // A creature brought back out of the old box (BRING BACK, removed with the
  // party and box). It is FROZEN: it does not age, cannot digivolve, and cannot
  // be lost. Nothing sets it any more, but a save made while one was live still
  // loads one, so the flag and every check on it stay.
  bool frozen = false;

  // The player's own name, alongside the badges and the streak: it belongs to
  // whoever is playing, not to the creature, so newEgg() must never clear it.
  char trainerName[12] = "";
  void renameTrainer(const char *n) {
    strncpy(trainerName, n, sizeof(trainerName) - 1);
    trainerName[sizeof(trainerName) - 1] = 0;
    save();
  }

  uint8_t avatar = 0;       // which player sprite, 0..3
  uint16_t badges = 0;      // bit n = boss n beaten on easy
  uint16_t badgesHard = 0;  // ... and on hard

  uint16_t badgeMask(bool hard) const { return hard ? badgesHard : badges; }
  bool hasBadge(uint8_t i, bool hard) const { return (badgeMask(hard) >> i) & 1; }
  void winBadge(uint8_t i, bool hard) {
    (hard ? badgesHard : badges) |= (uint16_t)(1u << i);
    save();
  }
  uint8_t badgeCount(bool hard) const {
    uint16_t v = badgeMask(hard);
    uint8_t n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
  }

  // Skill learning. becomeForm() rewinds learnCursor, and checkLearnGates() walks
  // this form's learnset up to its clock: specials at once, attachment skills at
  // their hour offset. A free slot fills silently; a full set queues an offer, and
  // the queue is saved, so an offer outlives a reboot. An offline catch-up that
  // crosses a dozen hours queues its offers instead of firing them at boot. A
  // skill dropped or skipped is gone: the cursor has passed it.
  uint8_t learnCursor = 0;
  uint16_t learnQueue[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  uint8_t learnQCount = 0;
  void checkLearnGates();
  bool hasLearnOffer() const { return learnQCount > 0; }
  uint16_t learnOffer() const { return learnQCount ? learnQueue[0] : 0; }
  void acceptLearn(uint8_t slot);   // put the pending skill into slot 0..3
  void declineLearn();
  // The same ceiling for every Digimon: no evolution may be unreachable because of IV.
  static uint8_t trMaxFor(uint8_t iv) { (void)iv; return TRAIN_MAX; }
  uint8_t trMaxAtk() const { return TRAIN_MAX; }
  uint8_t trMaxDef() const { return TRAIN_MAX; }
  uint8_t trMaxSpe() const { return TRAIN_MAX; }
  void play();
  void toggleLight();
  bool isNightHour() const;
  void setScreenOff(bool off);  // the sketch reports the PWR button
  bool screenIsOff = false;
  void dbgTick() { tick(); }   // tests drive minutes directly; tick() is private
  // Keep the legacy key in step too, so tests also cover migration saves.
  void dbgSetSeen(uint32_t e) { lastSeenEpoch = e; prefs.putUInt("seen", e); }
  uint8_t sleepAuto = SLEEP_NONE;   // who decided the current sleep state  // dormir / despertar
  void clean();
  void caress();  // tap the creature
  void eggTap();  // tap the egg: 3 taps and it hatches
  void newEgg();   // start from scratch with a random starter
  void release();  // soltar (pulsacion larga + confirmar)
  void syncClock(uint32_t nowEpoch);  // applies the time elapsed while powered off
  void setClock(uint32_t nowEpoch);   // sets the time without applying progression
  void startFarewell();  // also usable from the serial console (BYE)
  // Retire on demand. Before the farewell is earned this is the SAME ceremony,
  // but it hands the next
  // creature EVO_PENALTY_LEVELS on every digivolution threshold. Retiring one that
  // has already earned its farewell costs nothing: it is then just the button.
  void startRetire();
  bool canRetireNow() const;
  bool retireIsFree() const { return canFarewellNow(); }
  // Was the retire NOW IN PROGRESS an early one? THE single answer, asked by
  // the tests. Distinct from retireIsFree(), which asks
  // whether a retire started right now WOULD be free: this one remembers what the
  // retire that is already running actually was, and outlives the moment it began.
  bool retireIsEarly() const { return retirePending; }
  uint8_t evoPenalty() const { return evoPen; }
  void startRunaway();   // also usable from the serial console (RUN)

  bool isEgg() const { return speciesId < 0; }
  uint8_t eggCracks() const { return eggTaps; }
  bool eating() const { return millis() < eatUntil; }
  bool showHeart() const { return millis() < heartUntil; }
  bool evolving() const { return millis() < evolveUntil; }
  float evolveT() const {     // evolution animation progress 0..1
    uint32_t n = millis();
    uint32_t left = evolveUntil > n ? evolveUntil - n : 0;
    return 1.0f - (float)left / (float)EVOLVE_ANIM_MS;
  }
  bool canEvolveNow() const;  // a branch's conditions are met (ready)
  void evolve();              // triggers the transformation (called by a user tap)
  // De-digivolving: into any form that evolves into this one and is in the Digidex.
  // The earlier form then waits out its stage, and the partner's HP goes up.
  uint8_t devolveOptions(int16_t *out, uint8_t max) const;
  bool devolve(int16_t to);
  bool canFarewellNow() const;  // final form + 7 days: ready to say goodbye (button)
  // Total neglect RIGHT NOW. THE single answer: tick() counts against it and
  // canRunawayNow() re-checks it, because neglectTicks is frozen (neither
  // counted nor cleared) while asleep and so can outlive the state that
  // earned it -- see sleep_test.
  bool inTotalNeglect() const { return !fullness && !joy && !energy && !hygiene; }
  bool canRunawayNow() const;   // total abandonment for 1h: ready to run away (sad button)
  // Is there a form this creature could digivolve into that this player could be
  // SHOWN? Public so a test can drive it without reconstructing canEvolveNow().
  bool evoTargetShowable() const;
  // the user decides in a dialog; "keep/stay together" postpones and re-offers later
  bool wantEvolveButton() const { return canEvolveNow() && level() > evoDeclinedLv; }
  // "Keep form" re-offers next level, and the next time round the offer is the next
  // branch whose conditions hold: declining is how you pass over one for another.
  bool wantFarewellButton() const { return canFarewellNow() && ageMinutes >= farDeclinedAge; }
  void declineEvolve() { evoDeclinedLv = level(); evoBranch++; }  // re-offers on levelling up
  void declineFarewell() { farDeclinedAge = ageMinutes + 1440; } // re-offers within 1 day
  // first game: the player chooses a starter (PUNIMON/BOTAMON/POYOMON)
  bool awaitingStarter() const { return starterPick; }
  void chooseStarter(int16_t dex) { eggTarget = dex; starterPick = false; save(); }
  // Empties NVS; the caller restarts into a new game (serial command WIPE).
  // BOTH generation counters go back to zero with it, or the next save would
  // write a record numbered above the empty slots and the parity invariant in
  // ckptSlot() would no longer hold.
  //
  // It does NOT stop later saves. Keeping the in-RAM creature from being written
  // back over the wipe is the job of saveInhibited in the sketch, next to the
  // flush hooks that are the actual risk -- one guard, where the callers are.
  void factoryReset() {
    prefs.clear();
    saveGeneration = 0;
    playerGeneration = 0;
    saveFailures = 0;
    pendingSave = false;
  }
  void dbgRegister(int16_t dex) { registerSpecies(dex); }    // test: as if raised
  void dbgRunawayReady() { fullness = joy = energy = hygiene = 0; neglectTicks = RUNAWAY_TICKS; }  // test
  // test: force what the egg holds and hatch it now (serial command EGG).
  // The variant IV guarantee only fires inside hatch(), so without
  // this there is no way to exercise it from outside the class.
  void dbgHatchAs(int16_t dex, bool wantVariant) {
    if (dex < 1 || dex > DEX_COUNT) return;
    eggTarget = dex;
    eggVariant = wantVariant;
    starterPick = false;
    speciesId = -1;
    eggTaps = 0;
    hatch();
  }
  // Capped at 100, the series' own ceiling. The cap is not cosmetic: ageMinutes
  // is never clamped, so without it 1 + ageMinutes/60 overflows this uint8_t
  // past ~10.6 days -- and the RTC catches up to 2 weeks offline, which lands on
  // level 337 and wraps to 81. Reached at 4d 3h; farewell is merely *offered* at
  // 3 days (level 73), so declining it to raise a stronger battler is a real
  // choice rather than an accident.
  uint8_t level() const {
    uint32_t l = 1 + ageMinutes / MINUTES_PER_LEVEL;
    return l > MAX_LEVEL ? MAX_LEVEL : (uint8_t)l;
  }
  bool isRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && (dexReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  bool isVariantRegistered(int16_t dex) const {
    return dex >= 1 && dex <= DEX_COUNT && (dexVariantReg[(dex - 1) >> 3] & (1 << ((dex - 1) & 7)));
  }
  uint16_t registeredCount() const;
  bool lineHasUnregistered(int16_t base) const;
  int16_t pickEggSpecies();        // public so egg rolls can be simulated (EGGS)
  // What the waiting egg would hatch into. Hidden from the PLAYER, not from the
  // code: the serial console already simulates rolls, and the version tests have
  // to see which creature a switch landed on.
  int16_t eggPeek() const { return eggTarget; }
  // Are this route's conditions met right now? THE single answer: the stage timer,
  // the age minimum, Agent Rank and every stat minimum. evoOptions() and the card
  // ask it, so a card cannot promise a digivolution that never comes.
  bool routeMet(const TsEvo &e) const;
  // The level a route's clock opens at: its target's stage timer (a minimum), a
  // de-digivolved form's hold, and the day owed for an early retire. One sum, asked
  // by routeMet() and by the card's countdown.
  uint8_t routeClock(const TsEvo &e) const;
  // Every branch open right now, in the game's order. Returns how many.
  uint8_t evoOptions(int16_t *out, uint8_t max) const;
  // The branch on offer: one of evoOptions(), 0 if none. declineEvolve() moves it on.
  int16_t digiTarget() const;
  // The soonest any showable branch's clock opens, 0 if this form has none left.
  uint8_t evoNextLevel() const;
  uint8_t agentRank() const { return AGENT_RANK_STUB; }   // phase 2 replaces this
  uint8_t trainingTotal() const;   // trAtk + trDef + trSpe, capped at 255
  uint8_t lowestStat() const { return min(min(fullness, joy), min(energy, hygiene)); }
  PetMood mood() const;
  // progress of the farewell/runaway ceremony, 0..1 (to animate it)
  float ceremonyT() const {
    if (ceremony == CER_NONE) return 0.0f;
    uint32_t n = millis();
    uint32_t left = ceremonyUntil > n ? ceremonyUntil - n : 0;
    return 1.0f - (float)left / (float)CEREMONY_MS;
  }

  // streak / bond / medals / name
  void rename(const char *name);
  bool hasMedal(uint16_t m) const { return medals & m; }
  bool showMedal() const { return millis() < medalUntil; }
  bool showMilestone() const { return millis() < milestoneUntil; }
  int careBonus() const;  // egg improvement from streak + bond
  // Has today's care already been registered? THE single answer, so the UI does
  // not keep its own idea of when a day turns over: registerCare() decides that,
  // off the same clock, and a checklist disagreeing with the streak it is meant
  // to describe would be worse than not showing one.
  bool caredToday() const { return lastCareDay != 0 && lastCareDay == today(); }

  // deferred periodic save: tick() marks it pending and the loop flushes it
  // when the screen is dimmed/off (the flash write freezes
  // both cores for ~1s: this way it is neither seen nor cuts the touch)
  bool savePending() const { return pendingSave; }
  // Which generation the creature's checkpoint is on. Exposed READ-ONLY, for the
  // tests that check which slot a save landed in.
  uint32_t petGeneration() const { return saveGeneration; }
  // Is persistence actually WORKING? False means the creature on the panel is
  // not being written anywhere -- NVS would not open, or it is refusing writes
  // because it is full or failing. The firmware used to be unable to tell: the
  // only symptom was a line on a serial port no player has attached, so hours
  // of play could be lost with nothing on screen to suggest it. The sketch
  // draws a warning when this goes false, because a player who KNOWS saving is
  // broken can EXPORT, and one who does not cannot.
  //
  // Three consecutive failures rather than one: a single short write can be a
  // transient, three in a row is not coming back on its own.
  bool saveHealthy() const { return opened && saveFailures < 3; }
  void flushSave();
  // Writes NOW, whatever pendingSave says. flushSave() is `if (pendingSave)
  // save()`, so a console command that changed something the game had not
  // already marked dirty wrote nothing at all and quietly depended on the next
  // autosave to notice. Defined out of line: save() is private and declared
  // further down.
  void saveNow();

private:
  Preferences prefs;
  // Scratch pets used for opponents share the player's NVS namespace. Only a
  // Pet explicitly opened with begin() owns that persistent state.
  bool opened = false;
  // Consecutive failed checkpoint writes. Reset by the first success, so it
  // measures "is saving broken NOW" rather than counting a lifetime.
  uint8_t saveFailures = 0;
  // ONE COUNTER PER RECORD, not one shared between them. If the creature's
  // checkpoint succeeded and the player's did not, a shared counter would
  // advance anyway -- and the next player write, one higher again, would land
  // back on the slot holding the only complete player copy and overwrite the
  // very fallback the pair exists to provide. Each record's next generation has
  // to be its OWN last plus one for the parity invariant in ckptSlot() to hold.
  uint32_t saveGeneration = 0;
  uint32_t playerGeneration = 0;
  uint32_t lastTick = 0;
  uint32_t eatUntil = 0;
  uint32_t heartUntil = 0;
  uint32_t evolveUntil = 0;
  int16_t eggTarget = 1;       // hidden dex that will come out of the egg
  bool eggVariant = false;       // surprise rolled when the egg is created
  uint8_t eggTaps = 0;
  uint8_t mistakeCooldown = 0;
  uint8_t ticksSinceSave = 0;
  bool pendingSave = false;     // periodic save waiting to be flushed
  uint8_t evoDeclinedLv = 0;    // "keep form": do not offer evolution until the next level
  uint8_t evoBranch = 0;        // which open branch is on offer; RAM only, like the decline
  uint32_t farDeclinedAge = 0;  // "stay together": do not offer the farewell until this age
  bool starterPick = false;     // first game: waiting for the player to choose a starter
  uint8_t evoPen = 0;           // levels added to this creature's digivolution gate
  bool retirePending = false;   // an early retire is under way; newEgg() spends it
  uint8_t neglectTicks = 0;
  uint16_t goodTicks = 0;  // well-cared streak: forges DEF
  uint32_t ceremonyUntil = 0;
  uint8_t bondToday = 0;       // daily cap on bond gain
  uint32_t medalUntil = 0;     // on-screen medal celebration
  uint32_t milestoneUntil = 0; // streak milestone celebration

  uint32_t today() const { return lastSeenEpoch ? lastSeenEpoch / 86400 : 0; }
  void registerCare();   // first care of the day: streak + bond
  void addBond(uint8_t amt);
  uint8_t rollIV(int bonus) const;  // an 8-31 roll pushed by care
  void rollIVs();                   // all 4, with the variant guarantee
  uint16_t routeStat(uint8_t tsStat) const;
  void becomeForm(int16_t dex);     // digivolved or de-digivolved: what both share
  void skillGranted(uint16_t mv);   // a free slot fills silently, a full set offers
  void defTick(bool resting);       // calm forges defence (see pet.cpp)
  void checkMedals();
  void tick();
  void applyAutoSleep();
  void hatch();
  void registerSpecies(int16_t dex);
  bool saveCoreSnapshot();
  bool loadCoreSnapshot();
  bool savePlayerSnapshot();
  bool loadPlayerSnapshot();
  void save();
  void load();
  static uint8_t clamp100(int v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
};

#include "pet.h"
#include "avatars.h"
#include "noart.h"   // speciesHasArt(): the egg pool skips what cannot be drawn
#include "audio.h"
#include "ckpt.h"   // the alternating CRC-checked record format
#include <stddef.h>

// --- the creature ----------------------------------------------------------
// The Time Stranger cutover restarted this record: dex numbers and skill ids mean
// something else now, so it has a new magic and a v1.0.0 record ("TKP1") is never
// read as one. saveReplaceOld() in save.cpp is what deals with those.
static constexpr uint32_t PET_CORE_MAGIC = 0x32504B54UL;  // "TKP2"
static constexpr uint16_t PET_CORE_VERSION = 1;
// Generous enough to read a record from a later build; see ckptRead().
static constexpr size_t PET_CORE_CAP = PET_CKPT_CAP;

// The fixed part of the record. PetCoreSnapshot below is exactly this prefix, and
// only PET_FIXED bytes of it are ever copied -- the struct's own sizeof may be
// rounded up by its 4-byte alignment and those bytes are not part of the format.
static constexpr size_t PET_FIXED = 96;

struct PetCoreSnapshot {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t generation;
  uint32_t ageMinutes;
  uint32_t lastSeenEpoch;
  uint32_t formAgeMin;
  int16_t speciesId;
  int16_t eggTarget;
  uint16_t medals;
  uint16_t moves[MOVE_SLOTS];
  uint16_t learnQueue[8];
  uint8_t fullness, joy, energy, hygiene, poops, weight;
  uint8_t ivAtk, ivDef, ivSpe, ivHp;
  uint8_t trAtk, trDef, trSpe;
  uint8_t learnCursor, learnQCount;
  uint8_t berryKnown, variant, eggVariant, starterPick;
  uint8_t evoPen, sleepAuto, retirePending, eggTaps;
  uint8_t careMistakes, sleeping, lastEnd, frozen, bond;
  uint8_t deDigiCount, evoHold;
  char nick[12];
};
// EXACT, not "< 256". PET_FIXED is on-disk format -- it is what the crc covers
// and what a stored `size` implies -- so a field added above changes what every
// device already holds. Failing this assert is the reminder to read the
// append-only rule in ckpt.h, then append the field at the end and move the number.
static_assert(offsetof(PetCoreSnapshot, nick) == PET_FIXED - sizeof(PetCoreSnapshot::nick),
              "pet checkpoint fixed layout changed");
static_assert(sizeof(PetCoreSnapshot) == PET_FIXED, "PET_FIXED overruns the struct");
static_assert(PET_FIXED + CKPT_CRC <= PET_CORE_CAP, "raise PET_CORE_CAP");

// Takes a validated blob and copies whatever body prefix the stored record and
// this build have in common. Anything this build has that the record did not
// keeps its zero initialiser, and anything the record has that this build does
// not is dropped -- the append-only rule is what makes both directions safe.
static void petCoreFromBytes(const uint8_t *buf, uint16_t n,
                             PetCoreSnapshot &snapshot) {
  size_t body = n - CKPT_CRC;
  if (body > PET_FIXED) body = PET_FIXED;
  snapshot = PetCoreSnapshot();
  memcpy(&snapshot, buf, body);
}

static bool readPetCore(Preferences &prefs, const char *key,
                        PetCoreSnapshot &snapshot) {
  uint8_t buf[PET_CORE_CAP];
  uint16_t n = ckptRead(prefs, key, PET_CORE_MAGIC, buf, sizeof(buf));
  if (!n) return false;
  petCoreFromBytes(buf, n, snapshot);
  return true;
}

// --- the player ------------------------------------------------------------
// Everything in a save that OUTLIVES the creature: the tamer, the Digidex,
// the badges, the daily streak, the medal total and the minigame records. That
// was ~25 more keys written one after another, so a cut between them could
// leave a badge won without the dex entry that came with it, or a medal counted
// on the creature but not in the lifetime total. The same fault as issue #3,
// one layer out, and it survived the first fix because that fix only covered
// the creature.
//
// Unlike the creature's, this record CANNOT be a struct: dexReg is sized by
// DEX_COUNT, and a struct would move every field after it whenever the roster grew.
// So the DIMENSIONS TRAVEL IN THE HEADER and each array is copied by its own prefix
// rule: a stored array shorter than this build's lands at the front and the tail
// keeps its zero initialiser, a longer one is truncated.
//
// Restarted at the Time Stranger cutover, for the same reason as the creature's: dex
// bit n names a different Digimon now, and there is one ladder.
static constexpr uint32_t PLAYER_MAGIC = 0x32594B54UL;   // "TKY2"
static constexpr uint16_t PLAYER_VERSION = 1;
// Header + every scalar, before the arrays begin.
//
// THIS LENGTH TRAVELS IN THE RECORD, at buf[19], for exactly the reason the array
// dimensions do. Appending a scalar moves where the arrays start, and
// loadPlayerSnapshot() validates `need` against the stored body length -- so
// simply raising this number would make every existing record fail that check.
// So a future scalar goes at the end of the block, this number moves, and both
// directions keep working.
static constexpr size_t PLAYER_FIXED = 44;
static constexpr size_t PLAYER_CAP = PLAYER_CKPT_CAP;
static constexpr size_t PLAYER_DEX_BYTES = (DEX_COUNT + 7) / 8;
// DERIVED from the tables, never restated: CLAUDE.md § "A record's size is
// load-bearing" is what a literal here costs. Failing this is
// the signal to raise PLAYER_CAP, not to trim the record.
static constexpr size_t PLAYER_MAX =
    PLAYER_FIXED + 2 * PLAYER_DEX_BYTES + 12 /* trainerName */ + CKPT_CRC;
static_assert(PLAYER_MAX <= PLAYER_CAP, "raise PLAYER_CAP for the player record");

// A bounds-checked cursor over the variable tail of a blob. Overrunning sets
// `bad` rather than running off the end of the buffer.

bool Pet::savePlayerSnapshot() {
  const uint32_t nextGen = playerGeneration + 1;
  uint8_t buf[PLAYER_CAP] = {};

  ckptWr16(buf + 12, (uint16_t)sizeof(dexReg));   // u16: a dex byte count outgrew 255 long ago
  buf[16] = (uint8_t)sizeof(trainerName);
  buf[17] = avatar;
  buf[19] = (uint8_t)PLAYER_FIXED;                // the scalar block: where the arrays start
  ckptWr16(buf + 20, badges);
  ckptWr16(buf + 22, badgesHard);
  ckptWr16(buf + 24, streak);
  ckptWr16(buf + 26, bestStreak);
  ckptWr32(buf + 28, lastCareDay);
  ckptWr16(buf + 32, totalMedals);
  ckptWr16(buf + 34, lastMilestone);
  ckptWr16(buf + 36, gameHi);
  ckptWr16(buf + 38, strHi);
  ckptWr16(buf + 40, spdHi);
  ckptWr16(buf + 42, memoHi);

  CkptCursor cur{ buf, PLAYER_FIXED, sizeof(buf) - CKPT_CRC };
  cur.put(dexReg, sizeof(dexReg));
  cur.put(dexVariantReg, sizeof(dexVariantReg));
  cur.put(trainerName, sizeof(trainerName));
  if (cur.bad) {   // PLAYER_MAX static_asserts this cannot happen; say so if it does
    Serial.println("save: player record does not fit PLAYER_CAP");
    return false;
  }

  uint16_t total = ckptSeal(buf, cur.at, PLAYER_MAGIC, PLAYER_VERSION, nextGen);
  const char *key = ckptSlot(nextGen, "plyA", "plyB");
  const size_t written = prefs.putBytes(key, buf, total);
  if (written != total) {
    Serial.printf("save: %s wrote %u/%u bytes\n", key, (unsigned)written, (unsigned)total);
    return false;
  }
  // Read back into the same buffer and re-check, exactly as the creature's does.
  uint16_t n = ckptRead(prefs, key, PLAYER_MAGIC, buf, sizeof(buf));
  if (n != total || ckptRd32(buf + 8) != nextGen) {
    Serial.printf("save: %s read-back failed (%u/%u bytes)\n", key, (unsigned)n, (unsigned)total);
    return false;
  }
  playerGeneration = nextGen;
  return true;
}

bool Pet::loadPlayerSnapshot() {
  uint8_t buf[PLAYER_CAP];
  uint16_t n = ckptReadNewest(prefs, "plyA", "plyB", PLAYER_MAGIC, buf, sizeof(buf));
  if (!n) return false;

  const size_t dexBytes = ckptRd16(buf + 12);
  const size_t nameN = buf[16];
  // Check the dimensions BEFORE copying a single byte. A record that disagrees
  // with its own length is rejected whole rather than half-applied. A body LONGER
  // than the dimensions describe is fine and ignored: that is a later build's
  // appended field.
  const size_t body = n - CKPT_CRC;
  const size_t fixed = buf[19];
  if (fixed < PLAYER_FIXED || fixed > body) {
    Serial.println("save: player checkpoint scalar block length is not believable");
    return false;
  }
  const size_t need = fixed + 2 * dexBytes + nameN;
  if (need > body) {
    Serial.println("save: player checkpoint dimensions do not match its length");
    return false;
  }

  playerGeneration = ckptRd32(buf + 8);
  avatar = buf[17];
  badges = ckptRd16(buf + 20);
  badgesHard = ckptRd16(buf + 22);
  streak = ckptRd16(buf + 24);
  bestStreak = ckptRd16(buf + 26);
  lastCareDay = ckptRd32(buf + 28);
  totalMedals = ckptRd16(buf + 32);
  lastMilestone = ckptRd16(buf + 34);
  gameHi = ckptRd16(buf + 36);
  strHi = ckptRd16(buf + 38);
  spdHi = ckptRd16(buf + 40);
  memoHi = ckptRd16(buf + 42);

  CkptCursor cur{ buf, fixed, body };
  cur.take(dexReg, sizeof(dexReg), dexBytes);
  cur.take(dexVariantReg, sizeof(dexVariantReg), dexBytes);
  cur.take(trainerName, sizeof(trainerName), nameN);
  trainerName[sizeof(trainerName) - 1] = 0;

  if (avatar >= AVATAR_COUNT) avatar = 0;   // a save from when there were four
  return true;
}

void Pet::begin() {
  // CHECKED. This used to set opened = true regardless, so a namespace that
  // would not open left every write failing forever with nothing to show for it
  // -- getBool("init") returns its default, the player is handed a fresh egg,
  // and saving silently does nothing for as long as they keep playing. It is
  // still a fresh egg now, because there is nothing to load, but saveHealthy()
  // is false and the panel says so.
  opened = prefs.begin("tamadigi", false);
  if (!opened)
    Serial.println("save: NVS namespace would not open -- NOTHING WILL BE SAVED");
  saveGeneration = 0;
  playerGeneration = 0;
  // Zeroed BEFORE the branch below, not inside load(): getBytes() leaves its
  // destination untouched when the key is missing, and the fresh-install path
  // returns without ever calling load(). Without this a begin() after a factory
  // reset keeps the old Digidex alive in RAM -- the firmware reboots on WIPE so
  // it never showed there, but anything calling begin() twice would see it.
  memset(dexReg, 0, sizeof(dexReg));
  memset(dexVariantReg, 0, sizeof(dexVariantReg));
  if (!prefs.getBool("init", false)) {
    prefs.putBool("init", true);
    newEgg();
  } else {
    load();
  }
  lastTick = millis();
}

void Pet::newEgg() {
  ceremony = CER_NONE;
  neglectTicks = 0;
  weight = 0;
  speciesId = -1;
  prevSpeciesId = -1;
  eggTarget = pickEggSpecies();  // hidden species, leaning toward incomplete lines
  starterPick = (registeredCount() == 0);  // first game: the player chooses a starter
  // variant roll: 1/48 base, better after a farewell and with a high streak/bond
  int variantBase = (lastEnd == CER_FAREWELL ? 24 : 48) - careBonus();
  if (variantBase < 8) variantBase = 8;
  eggVariant = (random(variantBase) == 0);
  // The debt lands on the creature about to hatch, and is spent doing so --
  // it is a one-day penalty, not a running total that compounds each retire.
  evoPen = retirePending ? EVO_PENALTY_LEVELS : 0;
  retirePending = false;
  eggTaps = 0;
  fullness = 80;
  joy = 80;
  energy = 80;
  hygiene = 100;
  poops = 0;
  ageMinutes = 0;
  careMistakes = 0;
  mistakeCooldown = 0;
  sleeping = false;
  frozen = false;
  save();
}

// offline progression: time passed even while powered off, but with
// mercy - the bars drop with a floor of 15 (it comes back hungry, not dead),
// with no neglect and no running away while you were gone
static uint8_t dropTo(uint8_t v, uint8_t d, uint8_t fl) {
  if (v <= fl) return v;
  return (v - fl > d) ? v - d : fl;
}

// See the essay above ClockVerdict in pet.h for why each of these is a refusal
// rather than a clamp.
ClockVerdict clockVerdict(uint32_t seen, uint32_t now, uint32_t *minsOut) {
  if (minsOut) *minsOut = 0;
  // Not a time. The old code assigned this to lastSeenEpoch before testing it,
  // which is how an unreadable RTC permanently lost the reference.
  if (now == 0) return CLK_REJECT;
  if (now < CLOCK_EPOCH_FLOOR) return CLK_REJECT;
  if (!seen) return CLK_REBASE;                 // first ever reading: adopt it
  if (now + CLOCK_BACK_SLACK_SECS < seen) return CLK_REBASE;   // went backwards
  if (now <= seen) return CLK_REBASE;           // inside the drift slack
  const uint32_t mins = (now - seen) / 60;
  if (mins < 2) return CLK_REBASE;              // nothing worth applying
  if (mins > CLOCK_MAX_OFFLINE_MINS) return CLK_REBASE;   // a fault, not a holiday
  if (minsOut) *minsOut = mins;
  return CLK_APPLY;
}

// An explicit set: the player typing a time in SETTINGS, or a host sending TIME.
// It deliberately does NOT judge plausibility -- somebody said so on purpose --
// but it still will not adopt a non-time, because doing so is pure loss: the old
// reference is discarded and nothing is stored in its place.
void Pet::setClock(uint32_t nowEpoch) {
  if (!nowEpoch) return;
  lastSeenEpoch = nowEpoch;
  save();  // persist it now: a power cut does not lose the reference
}

void Pet::syncClock(uint32_t nowEpoch) {
  uint32_t mins = 0;
  const ClockVerdict verdict = clockVerdict(lastSeenEpoch, nowEpoch, &mins);
  if (verdict == CLK_REJECT) {
    // KEEP the reference. Nothing is written, so the next boot with a working
    // clock measures from when the device was really last seen.
    Serial.printf("clock: reading %u refused, keeping %u\n",
                  (unsigned)nowEpoch, (unsigned)lastSeenEpoch);
    return;
  }
  lastSeenEpoch = nowEpoch;
  if (verdict == CLK_REBASE) {
    // A real time, but not a measurable absence: adopt it and age NOTHING. This
    // is where an implausible jump lands, and it costs the player nothing -- age,
    // bond, training and medals are all untouched.
    if (mins == 0) Serial.printf("clock: rebased to %u\n", (unsigned)nowEpoch);
    save();
    return;
  }
  if (ceremony != CER_NONE || starterPick) {
    save();  // no time to apply, or still choosing a starter
    return;
  }

  for (uint32_t i = 0; i < mins; i++) {
    ageMinutes++;
    if (isEgg()) {
      if (ageMinutes >= 3) hatch();  // hatches while you are away
      continue;
    }
    if (sleeping) {  // rest: drops slowly and with a floor, same as live
      energy = clamp100(energy + 6);
      if (ageMinutes % 2 == 0) {
        fullness = dropTo(fullness, 1, 30);
        joy = dropTo(joy, 1, 35);
      }
      if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
      continue;
    }
    fullness = dropTo(fullness, 2, 15);
    energy = dropTo(energy, 1, 15);
    hygiene = dropTo(hygiene, 1, 15);
    joy = dropTo(joy, 1, 15);
  }
  if (!isEgg()) {
    if (!sleeping) {  // asleep it does not make a mess
      uint8_t p = poops + mins / 240;
      poops = p > 3 ? 3 : p;
    }
    // evolution is NOT applied offline: it stays ready and the user triggers it
    // by tapping the creature on returning (so they see the transformation)
  }
  Serial.printf("offline: %u min applied (lv.%u)\n", mins, level());
  save();
}

void Pet::update(uint32_t nowMs) {
  // end of the ceremony: the creature leaves and a new egg remains
  if (ceremony != CER_NONE && millis() > ceremonyUntil) {
    newEgg();
    return;
  }
  while (nowMs - lastTick >= PET_TICK_MS) {
    lastTick += PET_TICK_MS;
    tick();
  }
}

void Pet::tick() {
  if (ceremony != CER_NONE) return;  // time stops during the farewell
  if (starterPick) return;  // the game does not start until a starter is chosen: if
                            // time ran here, the egg would hatch by itself after
                            // 3 min with the rolled species and the player's
                            // choice would be lost
  if (!frozen) ageMinutes++;   // a revived companion does not age

  if (isEgg()) {
    if (ageMinutes >= 3) hatch();  // if you do not tap it, it hatches by itself after 3 min
    return;
  }

  applyAutoSleep();   // put down at 21:00 still goes to bed at 22:00


  // sleep is rest: energy recovers and the needs drop MUCH
  // more slowly than awake and with a floor (it wakes wanting a little attention, not at
  // zero, with no neglect or running away). awake: food -2/min, hyg/joy -1/min.
  // Weight still burns and the rest counts toward DEF (see defTick).
  if (sleeping) {
    energy = clamp100(energy + 6);
    if (weight > 0 && ageMinutes % 3 == 0) weight--;
    if (ageMinutes % 2 == 0) {                 // ~4x slower than awake
      fullness = dropTo(fullness, 1, 30);
      joy = dropTo(joy, 1, 35);
    }
    if (ageMinutes % 3 == 0) hygiene = dropTo(hygiene, 1, 45);
    defTick(true);  // resting is also well-being: it counts toward DEF
    checkMedals();  // it can still cross a level by age while asleep
    if (++ticksSinceSave >= 5) pendingSave = true;
    return;
  }

  if (ageMinutes % MINUTES_PER_LEVEL == 0) sfxPlay(SFX_LEVEL);  // levelled up (awake)

  fullness = clamp100(fullness - 2);
  energy = clamp100(energy - 1);
  if (fullness > 40 && poops < 3 && random(100) < 15) poops++;

  hygiene = clamp100(hygiene - 1 - 4 * poops);
  // being overweight makes it lazy: energy drops twice as fast
  if (weight > 50) energy = clamp100(energy - 1);
  if (weight > 0 && ageMinutes % 3 == 0) weight--;

  defTick(false);  // calm forges defence

  int dJoy = -1;
  if (fullness < 30) dJoy -= 2;
  if (hygiene < 30) dJoy -= 2;
  joy = clamp100(joy + dJoy);

  // Neglect: letting a stat hit rock bottom counts as a care
  // mistake (with a cooldown so the same neglect is not counted every minute)
  if (mistakeCooldown > 0) mistakeCooldown--;
  if (lowestStat() <= 10 && mistakeCooldown == 0) {
    careMistakes++;
    mistakeCooldown = 60;
    if (bond > 1) bond--;  // neglect cools the bond, but without wiping it out:
                           // at -3 every 30 min far more was lost than could
                           // be gained in a whole day, and the bond got stuck
  }

  checkMedals();  // evolution is triggered by the user (canEvolveNow + tap), not by the tick
  checkLearnGates();

  // total abandonment: with EVERYTHING at zero for an hour it becomes ready to run away;
  // it does NOT leave by itself, the user triggers it with the button (a sad ending, they witness it)
  if (inTotalNeglect()) {
    if (neglectTicks < RUNAWAY_TICKS) neglectTicks++;
  } else {
    neglectTicks = 0;  // a single bit of care saves it
  }

  // full cycle (final form + 7 days): the farewell does NOT fire by itself; it stays
  // ready (canFarewellNow) and the user triggers it with the button, so they see it

  // periodic autosave: do NOT write to flash here (this runs inside the loop,
  // while animating); just mark it and let the loop flush it when the screen dims
  if (++ticksSinceSave >= 5) pendingSave = true;
}

// flushes the pending periodic save (the loop calls it at a moment with no
// animation so the flash-write stall is not visible)
void Pet::saveNow() { save(); }

void Pet::flushSave() {
  if (pendingSave) save();
}

// Calm forges defence: each hour of well-being (resting, or awake
// with everything >= 40) gives +1 DEF, up to the cap the IV allows.
//
// It used to require 12 STRAIGHT hours and ANY slip reset the counter, besides
// not counting sleep. Simulating a whole life (3 days), that gave 1 point to the
// theoretically perfect player (one who acts every minute for 72 h) and 0 to
// everyone else, including one who checks in every 15 min: food drops 2/min,
// so anyone who does not visit the creature every half hour is ALWAYS below
// 40 and the counter never started. DEF was, in practice, untrainable.
// Now it accumulates instead of resetting: a slip costs the bad minutes, not
// all the progress.
void Pet::defTick(bool resting) {
  if (!resting && lowestStat() < 40) return;
  if (++goodTicks < DEF_TRAIN_TICKS) return;
  goodTicks = 0;
  if (trDef < trMaxDef()) trDef++;
}

// Does any Digimon reachable from this base (through every branch) remain
// unregistered? Walks the routes breadth-first. All int16_t: the queue holds up to
// the whole roster, and a form leads to dozens.
bool Pet::lineHasUnregistered(int16_t base) const {
  if (!dexValid(base)) return false;
  int16_t q[DEX_COUNT + 1];
  bool seen[DEX_COUNT + 1] = { false };
  int16_t head = 0, tail = 0;
  q[tail++] = base;
  seen[base] = true;
  while (head < tail) {
    int16_t cur = q[head++];
    if (speciesHasArt(cur) && !isRegistered(cur)) return true;
    const TsDexEntry &d = TS_DEX_TBL[cur];
    for (uint16_t i = d.evoFirst; i < (uint16_t)d.evoFirst + d.evoCount; i++) {
      int16_t t = (int16_t)TS_EVO_TBL[i].to;
      if (dexValid(t) && !seen[t]) { seen[t] = true; q[tail++] = t; }
    }
  }
  return false;
}

// The training total the card shows: everything the three trainable stats have
// gathered, capped to a byte.
uint8_t Pet::trainingTotal() const {
  uint16_t t = (uint16_t)trAtk + trDef + trSpe;
  return t > 255 ? 255 : (uint8_t)t;
}

uint8_t Pet::routeClock(const TsEvo &e) const {
  uint16_t at = TS_STAGE_ENTRY[TS_DEX_TBL[e.to].stage % STAGE_COUNT];
  if (evoHold > at) at = evoHold;
  at += evoPen;
  return at > 255 ? 255 : (uint8_t)at;
}

uint16_t Pet::routeStat(uint8_t tsStat) const {
  switch (tsStat) {
    case TS_STAT_HP:   return vitStat();
    case TS_STAT_ATK:  return atkStat();
    case TS_STAT_DEF:  return defStat();
    case TS_STAT_SPD:  return speStat();
    case TS_STAT_MATK: return spaStat();
    default:           return spdStat();
  }
}

bool Pet::routeMet(const TsEvo &e) const {
  if (level() < routeClock(e) || level() < e.minLevel) return false;
  if (agentRank() < e.rank) return false;
  for (uint8_t i = 0; i < TS_STAT_COUNT; i++)
    if (e.min[i] && routeStat(i) < tsScaled(e.min[i])) return false;
  return true;
}

uint8_t Pet::evoOptions(int16_t *out, uint8_t max) const {
  if (isEgg() || !dexValid(speciesId)) return 0;
  const TsDexEntry &d = TS_DEX_TBL[speciesId];
  uint8_t n = 0;
  for (uint16_t i = d.evoFirst; i < (uint16_t)d.evoFirst + d.evoCount && n < max; i++) {
    const TsEvo &e = TS_EVO_TBL[i];
    if (speciesShowable((int16_t)e.to) && routeMet(e)) out[n++] = (int16_t)e.to;
  }
  return n;
}

// THE digivolution answer. 0 means none is open -- a form whose branches ask for
// more stays as it is until training or age opens one.
int16_t Pet::digiTarget() const {
  int16_t open[EVO_OPTIONS_MAX];
  const uint8_t n = evoOptions(open, EVO_OPTIONS_MAX);
  return n ? open[evoBranch % n] : 0;
}

uint8_t Pet::evoNextLevel() const {
  if (isEgg() || !dexValid(speciesId)) return 0;
  const TsDexEntry &d = TS_DEX_TBL[speciesId];
  uint8_t best = 0;
  for (uint16_t i = d.evoFirst; i < (uint16_t)d.evoFirst + d.evoCount; i++) {
    const TsEvo &e = TS_EVO_TBL[i];
    if (!speciesShowable((int16_t)e.to)) continue;
    uint8_t at = routeClock(e);
    if (e.minLevel > at) at = e.minLevel;
    if (!best || at < best) best = at;
  }
  return best;
}

bool gArtPack = true;   // everything, until the SD says otherwise

// THE single answer to "can this player be shown this Digimon". The egg pool and
// every digivolution ask it; having two opinions is how TENTOMON's branch was
// filtered for years while the linear path was not.
bool speciesShowable(int16_t d) {
  return dexValid(d) && speciesHasArt(d) && gArtPack;
}

int16_t artProbe(uint8_t slot) {
  if (slot > 2) return 0;
  uint8_t seen = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (!speciesHasArt(d)) continue;
    const int16_t want = slot == 0 ? 0 : (slot == 1 ? NO_ART_DRAWABLE / 2 : NO_ART_DRAWABLE - 1);
    if (seen++ == want) return d;
  }
  return 0;
}

uint8_t starterForms(int16_t *out, uint8_t max) {
  uint8_t n = 0;
  for (int16_t d = 1; d <= DEX_COUNT; d++) {
    if (TS_DEX_TBL[d].stage != TS_ST_IN_TRAINING_1 || !speciesShowable(d)) continue;
    if (n < max) out[n] = d;
    n++;
  }
  return n;
}

// The egg: a random In-Training I, leaning toward lines with Digimon not raised yet.
// A stand-in for phase 2's egg rules, which keep this lean and drop what is left of
// the old tiers. With no pack on the card the table is still the answer: it is what
// the first egg of a board with no SD has always been.
int16_t Pet::pickEggSpecies() {
  int16_t pool[TS_DEX_COUNT];
  const uint8_t n = starterForms(pool, sizeof(pool) / sizeof(pool[0]));
  if (!n) {
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (TS_DEX_TBL[d].stage == TS_ST_IN_TRAINING_1 && speciesHasArt(d)) return d;
    return 1;
  }
  int16_t fresh[TS_DEX_COUNT];
  uint8_t f = 0;
  for (uint8_t i = 0; i < n; i++)
    if (lineHasUnregistered(pool[i])) fresh[f++] = pool[i];
  return f ? fresh[random(f)] : pool[random(n)];
}

void Pet::registerSpecies(int16_t dex) {
  if (dex < 1 || dex > DEX_COUNT) return;
  dexReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
  if (variant) dexVariantReg[(dex - 1) >> 3] |= (1 << ((dex - 1) & 7));
}

// streak and bond improve the egg roll (0..~14)
int Pet::careBonus() const {
  int s = streak > 30 ? 30 : streak;
  return s / 3 + bond / 25;
}

// first care of the day: advances the streak and strengthens the bond
void Pet::registerCare() {
  if (isEgg() || ceremony != CER_NONE) return;
  uint32_t d = today();
  if (d == 0 || d == lastCareDay) return;  // no clock, or it already counted today
  if (lastCareDay == 0 || d == lastCareDay + 1) {
    streak++;
  } else {
    streak = 1;        // there was a gap of days
    lastMilestone = 0;
  }
  lastCareDay = d;
  bondToday = 0;
  if (streak > bestStreak) bestStreak = streak;
  bond = clamp100(bond + 4);
  uint16_t ms = (streak >= 100) ? 100 : (streak >= 30) ? 30
              : (streak >= 7)   ? 7   : (streak >= 3)  ? 3 : 0;
  if (ms > lastMilestone) {
    lastMilestone = ms;
    milestoneUntil = millis() + 4500;
  }
  checkMedals();
  save();
}

void Pet::addBond(uint8_t amt) {
  if (bondToday >= 20) return;  // daily cap: the bond cannot be farmed
  bond = clamp100(bond + amt);
  bondToday += amt;
}

void Pet::checkMedals() {
  if (isEgg()) return;
  uint16_t before = medals;
  if (level() >= 10) medals |= MED_LV10;
  if (level() >= 25) medals |= MED_LV25;
  if (level() >= 50) medals |= MED_LV50;
  if (berryKnown) medals |= MED_BERRY;
  if (streak >= 7) medals |= MED_STREAK7;
  if (bond >= 100) medals |= MED_BOND;
  if (TS_DEX_TBL[speciesId].evoCount == 0) medals |= MED_FINAL;
  if (weight == 0 && level() >= 5 && careMistakes == 0) medals |= MED_FIT;
  uint16_t gained = medals & ~before;
  if (gained) {
    for (uint16_t m = gained; m; m &= (m - 1)) totalMedals++;
    newMedal = gained;
    medalUntil = millis() + 4000;
    if (!sleeping) sfxPlay(SFX_MEDAL);
    save();
  }
}

void Pet::rename(const char *name) {
  strncpy(nick, name, sizeof(nick) - 1);
  nick[sizeof(nick) - 1] = 0;
  save();
}

// Approach B: Time Stranger's own growth, scaled, plus training. A level-1 Digimon
// with no training is exactly its table value over ten, so a stat always reads as
// Time Stranger's number at the same level, plus what the partner has earned.
uint16_t petCalcStat(int16_t dex, uint8_t stat, uint8_t lvl, uint8_t tr) {
  return (uint16_t)(tsScaled(tsGrowth(dex, stat, lvl)) + tr);
}

uint16_t petCalcHp(int16_t dex, uint8_t lvl, uint8_t deDigiCount) {
  uint32_t pct = 100 + (uint32_t)deDigiCount * DEDIGI_HP_STEP_PCT;
  if (pct > DEDIGI_HP_MAX_PCT) pct = DEDIGI_HP_MAX_PCT;
  uint32_t hp = tsScaled(tsGrowth(dex, TS_STAT_HP, lvl)) * pct / 100;
  return hp ? (uint16_t)hp : 1;
}

uint8_t trainGainFor(uint8_t raw, uint8_t iv) {
  const uint16_t pct = 70 + (60 * (uint16_t)iv + 15) / 31;
  return (uint8_t)(((uint16_t)raw * pct + 50) / 100);
}

uint16_t Pet::atkStat() const {
  return isEgg() ? 0 : petCalcStat(speciesId, TS_STAT_ATK, level(), trAtk);
}
uint16_t Pet::defStat() const {
  return isEgg() ? 0 : petCalcStat(speciesId, TS_STAT_DEF, level(), trDef);
}
uint16_t Pet::speStat() const {
  return isEgg() ? 0 : petCalcStat(speciesId, TS_STAT_SPD, level(), trSpe);
}
// HP is not trained: it grows with age, and the de-digivolve boost multiplies it.
uint16_t Pet::vitStat() const {
  return isEgg() ? 0 : petCalcHp(speciesId, level(), deDigiCount);
}
// Magic reuses the physical training, which is what keeps ATK and DEF training
// worth doing for a Digimon that fights with magic.
uint16_t Pet::spaStat() const {
  return isEgg() ? 0 : petCalcStat(speciesId, TS_STAT_MATK, level(), trAtk);
}
uint16_t Pet::spdStat() const {
  return isEgg() ? 0 : petCalcStat(speciesId, TS_STAT_MDEF, level(), trDef);
}

// ---------- skills ----------

uint8_t Pet::moveCount() const {
  uint8_t n = 0;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (moves[i]) n++;
  return n;
}

bool Pet::knowsMove(uint16_t mv) const {
  if (!mv) return false;
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (moves[i] == mv) return true;
  return false;
}

uint8_t Pet::formHours() const {
  uint32_t h = ageMinutes > formAgeMin ? (ageMinutes - formAgeMin) / MINUTES_PER_LEVEL : 0;
  return h > 255 ? 255 : (uint8_t)h;
}

// A skill arriving: a free slot fills silently, a full set makes an offer. Already
// in a slot is skipped; an offer that no longer fits the queue is skipped too, and
// a skipped skill is gone, as one the player declined would be.
void Pet::skillGranted(uint16_t mv) {
  if (!skillValid(mv) || knowsMove(mv)) return;
  for (int s = 0; s < MOVE_SLOTS; s++)
    if (!moves[s]) { moves[s] = mv; return; }
  if (learnQCount >= sizeof(learnQueue) / sizeof(learnQueue[0])) return;
  for (uint8_t q = 0; q < learnQCount; q++)
    if (learnQueue[q] == mv) return;
  learnQueue[learnQCount++] = mv;
}

// Walks this form's learnset up to its clock. The cursor is why a skill is offered
// once: it never goes back, except on becoming a form again.
void Pet::checkLearnGates() {
  if (isEgg() || ceremony != CER_NONE || !dexValid(speciesId)) return;
  const TsDexEntry &d = TS_DEX_TBL[speciesId];
  const uint8_t hours = formHours();
  bool moved = false;
  while (learnCursor < d.learnCount &&
         TS_LEARN_TBL[d.learnFirst + learnCursor].hours <= hours) {
    skillGranted(TS_LEARN_TBL[d.learnFirst + learnCursor].skill);
    learnCursor++;
    moved = true;
  }
  if (moved) pendingSave = true;
}

void Pet::relearnFromLevel() {
  for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = 0;
  learnQCount = 0;
  learnCursor = 0;
  if (isEgg() || !dexValid(speciesId)) return;
  const TsDexEntry &d = TS_DEX_TBL[speciesId];
  const uint8_t hours = formHours();
  while (learnCursor < d.learnCount && TS_LEARN_TBL[d.learnFirst + learnCursor].hours <= hours)
    learnCursor++;
  // Specials first, in order; then the newest attachment skills, from the back.
  uint8_t n = 0;
  for (uint8_t i = 0; i < learnCursor && n < MOVE_SLOTS; i++)
    if (!TS_LEARN_TBL[d.learnFirst + i].hours) moves[n++] = TS_LEARN_TBL[d.learnFirst + i].skill;
  for (int i = (int)learnCursor - 1; i >= 0 && n < MOVE_SLOTS; i--)
    if (TS_LEARN_TBL[d.learnFirst + i].hours) moves[n++] = TS_LEARN_TBL[d.learnFirst + i].skill;
}

static void popLearn(uint16_t *q, uint8_t &n) {
  if (!n) return;
  for (uint8_t i = 0; i + 1 < n; i++) q[i] = q[i + 1];
  q[--n] = 0;
}

void Pet::acceptLearn(uint8_t slot) {
  if (!learnQCount || slot >= MOVE_SLOTS) return;
  moves[slot] = learnQueue[0];
  popLearn(learnQueue, learnQCount);
  save();
}

void Pet::declineLearn() {
  popLearn(learnQueue, learnQCount);
  save();
}

uint8_t Pet::pendingLearnables(uint16_t *out, uint8_t max) const {
  if (isEgg() || !out || !max || !dexValid(speciesId)) return 0;
  const TsDexEntry &d = TS_DEX_TBL[speciesId];
  const uint8_t hours = formHours();
  uint8_t w = 0;
  for (uint8_t i = 0; i < d.learnCount && w < max; i++) {
    const TsLearn &l = TS_LEARN_TBL[d.learnFirst + i];
    if (l.hours > hours) break;
    if (!knowsMove(l.skill)) out[w++] = l.skill;
  }
  return w;
}

// An IV roll: 8-31. The floor at 8 is deliberate: each raising lasts 3 days and a
// throwaway individual would be a disproportionate punishment. The streak and bond
// of the PREVIOUS creature push the roll: good care improves the brood.
uint8_t Pet::rollIV(int bonus) const {
  int v = 8 + (int)random(24) + bonus / 2;  // bonus 0..14 -> +0..7
  return (uint8_t)(v > 31 ? 31 : v);
}

void Pet::rollIVs() {
  int bonus = careBonus();
  ivAtk = rollIV(bonus);
  ivDef = rollIV(bonus);
  ivSpe = rollIV(bonus);
  ivHp = rollIV(bonus);
  // in gen 2 the variant WAS a specific DV pattern: a variant was never
  // mediocre. Here it translates into a floor of 20 on every IV.
  if (variant) {
    if (ivAtk < 20) ivAtk = 20;
    if (ivDef < 20) ivDef = 20;
    if (ivSpe < 20) ivSpe = 20;
    if (ivHp < 20) ivHp = 20;
  }
}

uint16_t Pet::registeredCount() const {
  uint16_t n = 0;
  for (int i = 1; i <= DEX_COUNT; i++)
    if (isRegistered(i)) n++;
  return n;
}

// a final form that has completed its cycle (7 days): ready to say goodbye. The
// farewell is triggered by the user with the button (it does not fire by itself, so they see it)
bool Pet::canFarewellNow() const {
  if (frozen) return false;     // a companion cannot be lost; that is the point
  // Any Digimon that has lived its three days and grown past Rookie: on the
  // V-Pet a Champion that never won its battles still reaches the end of its
  // life, it just never becomes an Ultimate.
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         TS_DEX_TBL[speciesId].stage >= TS_ST_CHAMPION && ageMinutes >= FAREWELL_AGE_MIN;
}

// total abandonment for 1h: ready to run away. The user triggers it with the
// button (a sad ending); a single tick of care saves it (neglectTicks resets)
bool Pet::canRunawayNow() const {
  if (frozen) return false;
  // inTotalNeglect() as well as the counter, and NOT just the counter. The
  // sleeping branch of tick() returns before the neglect block, so neglectTicks
  // is frozen rather than cleared for the whole night: a creature that went to
  // bed at zero woke with energy back at 100 and was still one tap from
  // leaving, until the next tick 60 s later cleared it. That tap is a caress --
  // the button is drawn over the creature -- so the window really was reachable
  // and it cost somebody a AGUMON.
  return !isEgg() && !sleeping && ceremony == CER_NONE &&
         neglectTicks >= RUNAWAY_TICKS && inTotalNeglect();
}

bool Pet::canRetireNow() const {
  if (frozen) return false;     // a companion is never given up
  return !isEgg() && !sleeping && ceremony == CER_NONE && !starterPick;
}

// The ceremony is the same one; only the debt differs. Marked BEFORE the
// ceremony starts and spent by newEgg(), so a reset mid-ceremony loses the
// penalty rather than applying it to a creature that never got retired.
void Pet::startRetire() {
  if (!canRetireNow()) return;
  retirePending = !canFarewellNow();
  save();
  startFarewell();
  // An early retire is NOT the good ending and must not pay like one.
  // startFarewell() sets lastEnd = CER_FAREWELL, which blesses the next egg --
  // rare 27% -> 45%, Mega 3% -> 10%, variant 1/48 -> 1/24. Combined with the
  // creature no longer being banked, that made retiring early a pure VARIANT FARM:
  // retire, check the egg, retire again, with nothing accumulating to regret.
  // Neutral instead, exactly like a release. The ceremony on screen is still the
  // farewell -- this was a choice the player made, not a neglected creature
  // walking out -- but the reward is not.
  if (retirePending) {
    lastEnd = CER_RELEASE;
    save();
  }
}

void Pet::startFarewell() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_FAREWELL;
  ceremony = CER_FAREWELL;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;  // hearts throughout the farewell
  sfxPlay(SFX_BYE);
  save();
}

void Pet::startRunaway() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_RUNAWAY;
  ceremony = CER_RUNAWAY;
  ceremonyUntil = millis() + CEREMONY_MS;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::release() {
  if (isEgg() || ceremony != CER_NONE) return;
  lastEnd = CER_RELEASE;
  ceremony = CER_RELEASE;
  ceremonyUntil = millis() + CEREMONY_MS;
  heartUntil = ceremonyUntil;
  sfxPlay(SFX_BYE);
  save();
}

void Pet::hatch() {
  speciesId = eggTarget;
  variant = eggVariant;
  // the individual's IVs (each raising is unique). They are rolled BEFORE resetting the
  // bond on purpose: the careBonus that pushes them is the previous creature's.
  rollIVs();
  trAtk = trDef = trSpe = 0;
  deDigiCount = 0;
  evoHold = 0;
  goodTicks = 0;
  berryKnown = false;
  bond = 0;          // bond, medals and name belong to the individual
  bondToday = 0;
  medals = 0;
  newMedal = 0;
  nick[0] = 0;
  registerSpecies(speciesId);  // raised = registered in the digidex
  // Start empty: becoming the form is what hands over its specials.
  for (int i = 0; i < MOVE_SLOTS; i++) moves[i] = 0;
  memset(learnQueue, 0, sizeof(learnQueue));
  learnQCount = 0;
  learnCursor = 0;
  formAgeMin = ageMinutes;
  checkLearnGates();
  checkMedals();     // in case it is born with no branch ahead of it
  sfxPlay(SFX_HATCH);
  save();
}

// Is a digivolution ready to be offered? Each route's conditions decide, stage
// timers included: a timer is a minimum, and nothing here happens on its own. The
// player taps to watch it (evolve()), and "keep form" re-offers next level.
bool Pet::canEvolveNow() const {
  if (frozen) return false;     // frozen at the form it was banked in
  if (isEgg() || sleeping || ceremony != CER_NONE) return false;
  return digiTarget() != 0;
}

bool Pet::evoTargetShowable() const { return digiTarget() != 0; }

// What digivolving and de-digivolving share: the form changes, its specials arrive
// at once, and its attachment skills start counting from now. Skills in the slots
// carry over; care mistakes count again from zero, as on the V-Pet.
void Pet::becomeForm(int16_t dex) {
  prevSpeciesId = speciesId;
  speciesId = dex;
  registerSpecies(speciesId);
  careMistakes = 0;
  evoBranch = 0;
  evoDeclinedLv = 0;
  formAgeMin = ageMinutes;
  learnCursor = 0;
  checkLearnGates();
  checkMedals();
  sfxPlay(SFX_EVOLVE);
  evolveUntil = millis() + EVOLVE_ANIM_MS;
  save();
}

void Pet::evolve() {
  if (!canEvolveNow()) return;
  int16_t next = digiTarget();
  if (!next) return;
  evoHold = 0;
  becomeForm(next);
}

// A route read backwards. Routes are grouped by their source, so finding the ones
// that END here means walking all of them; this runs on a tap, not in a frame.
uint8_t Pet::devolveOptions(int16_t *out, uint8_t max) const {
  if (frozen || isEgg() || sleeping || ceremony != CER_NONE || !dexValid(speciesId)) return 0;
  uint8_t n = 0;
  for (uint16_t i = 0; i < TS_EVO_COUNT && n < max; i++) {
    const TsEvo &e = TS_EVO_TBL[i];
    if ((int16_t)e.to != speciesId || !isRegistered((int16_t)e.from)) continue;
    if (!speciesShowable((int16_t)e.from)) continue;
    bool dup = false;
    for (uint8_t j = 0; j < n; j++)
      if (out[j] == (int16_t)e.from) dup = true;
    if (!dup) out[n++] = (int16_t)e.from;
  }
  return n;
}

bool Pet::devolve(int16_t to) {
  int16_t open[EVO_OPTIONS_MAX];
  const uint8_t n = devolveOptions(open, EVO_OPTIONS_MAX);
  bool known = false;
  for (uint8_t i = 0; i < n; i++)
    if (open[i] == to) known = true;
  if (!known) return false;
  // The earlier form waits out its stage: as long as that stage lasts on the
  // clock, from now. Mega+ has no stage after it, so it borrows the last gap.
  const uint8_t st = TS_DEX_TBL[to].stage % STAGE_COUNT;
  const uint8_t gap = st + 1 < STAGE_COUNT ? TS_STAGE_ENTRY[st + 1] - TS_STAGE_ENTRY[st]
                                           : TS_STAGE_ENTRY[st] - TS_STAGE_ENTRY[st - 1];
  const uint16_t hold = (uint16_t)level() + gap;
  evoHold = hold > 255 ? 255 : (uint8_t)hold;
  if (deDigiCount < DEDIGI_COUNT_MAX) deDigiCount++;
  becomeForm(to);
  return true;
}

void Pet::feed() {
  feedBerry(0);
}

void Pet::feedBerry(uint8_t color) {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  if (lovesBerry(color)) {
    fullness = clamp100(fullness + 35);
    joy = clamp100(joy + 10);
    heartUntil = millis() + HEART_MS;  // "loves it!"
    berryKnown = true;                 // discovered: shown on the card
    addBond(2);
  } else {
    fullness = clamp100(fullness + 25);
  }
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

void Pet::feedCandy() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  fullness = clamp100(fullness + 10);
  joy = clamp100(joy + 12);
  weight = clamp100(weight + 12);  // treats take their toll
  eatUntil = millis() + EAT_ANIM_MS;
  registerCare();
  save();
}

uint8_t Pet::playResult(uint8_t score) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  // The ball game is DEFENCE's tamer now. It used to train SPEED, which was
  // moved to its own reaction test to stop playing being a stat grind -- and
  // that left DEF with no active tamer at all, only the slow passive tick.
  // Keeping the ball on the defensive stat fits it: you are stopping something
  // from getting past you.
  uint8_t before = trDef;
  uint8_t gain = score / 2;
  if (gain > 18) gain = 18;          // the same per-session ceiling as the bag
  gain = trainGainFor(gain, ivDef);
  uint16_t v = (uint16_t)trDef + gain;
  trDef = v > trMaxDef() ? trMaxDef() : (uint8_t)v;
  gain = trDef - before;
  joy = clamp100(joy + 5 + (score > 15 ? 30 : score * 2));
  energy = dropTo(energy, 10 + score / 2, 5);
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - score * 2;  // exercise burns weight
  weight = burn > 0 ? burn : 0;
  if (score >= 5) heartUntil = millis() + HEART_MS;
  if (score > gameHi) gameHi = score;  // nuevo record
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

// training bag: the hits train strength. Returns the gain.
uint8_t Pet::rewardTraining(uint8_t amount, uint8_t &which) {
  which = 0;
  if (ceremony != CER_NONE || isEgg() || !amount) return 0;
  // Only the stats with headroom are candidates.
  uint8_t room[3], n = 0;
  if (trAtk < trMaxAtk()) room[n++] = 0;
  if (trDef < trMaxDef()) room[n++] = 1;
  if (trSpe < trMaxSpe()) room[n++] = 2;
  if (!n) return 0;                     // nothing left to train
  which = room[random(n)];
  uint8_t before, capped;
  switch (which) {
    case 0: before = trAtk; capped = trMaxAtk(); trAtk = (uint8_t)min<uint16_t>(before + amount, capped); amount = trAtk - before; break;
    case 1: before = trDef; capped = trMaxDef(); trDef = (uint8_t)min<uint16_t>(before + amount, capped); amount = trDef - before; break;
    default: before = trSpe; capped = trMaxSpe(); trSpe = (uint8_t)min<uint16_t>(before + amount, capped); amount = trSpe - before; break;
  }
  save();
  return amount;
}

uint8_t Pet::trainSpeed(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 2;          // ~2 reactions = 1 point
  if (gain > 18) gain = 18;         // same per-session ceiling as the bag
  gain = trainGainFor(gain, ivSpe);
  uint8_t before = trSpe;
  uint16_t v = (uint16_t)trSpe + gain;
  trSpe = v > trMaxSpe() ? trMaxSpe() : (uint8_t)v;
  gain = trSpe - before;
  energy = dropTo(energy, 10, 5);
  fullness = dropTo(fullness, 4, 5);
  int burn = (int)weight - hits / 2;
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 4);
  if (hits > spdHi) spdHi = hits;
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

uint8_t Pet::trainStrength(uint16_t hits) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  uint8_t gain = hits / 4;          // ~4 hits = 1 training point
  if (gain > 18) gain = 18;         // per-session cap: STR is forged slowly
  gain = trainGainFor(gain, ivAtk);
  uint8_t before = trAtk;
  uint16_t v = (uint16_t)trAtk + gain;
  trAtk = v > trMaxAtk() ? trMaxAtk() : (uint8_t)v;
  gain = trAtk - before;            // what actually went up (it may hit the cap)
  energy = dropTo(energy, 12, 5);   // cansa
  fullness = dropTo(fullness, 5, 5);
  int burn = (int)weight - hits / 3;  // also burns weight
  weight = burn > 0 ? burn : 0;
  joy = clamp100(joy + 6);
  if (hits >= 20) heartUntil = millis() + HEART_MS;
  if (hits > strHi) strHi = hits;   // hits record
  // Training bonds, and it scales with the session: a token effort is worth the
  // base, a full one is worth more. The daily cap in addBond() still stops it
  // being farmed -- this changes how fast a good session gets there, not the
  // ceiling.
  addBond((uint8_t)(2 + gain / 6));
  registerCare();
  save();
  return gain;
}

// The memory game. JOY AND BOND, no stat -- see the note in pet.h. Modelled on
// play() rather than on the three tamers, and it obeys the same contract they
// all do: nothing while an egg or a ceremony is in progress, the daily bond cap
// still applies, the day is registered, and it saves.
uint8_t Pet::playMemo(uint16_t best) {
  if (ceremony != CER_NONE || isEgg()) return 0;
  // A longer sequence is worth more, with a ceiling: the game gets harder on its
  // own, so the reward does not also need to run away.
  uint8_t gain = (uint8_t)(best * 4);
  if (gain > 40) gain = 40;
  const uint8_t before = joy;
  joy = clamp100(joy + gain);
  gain = joy - before;
  // Concentration is tiring but not physical -- it costs less than the bag and
  // burns no weight, which is what keeps this the game you play when the creature
  // is too worn out for the others.
  energy = dropTo(energy, 4, 5);
  fullness = dropTo(fullness, 2, 5);
  if (best >= 4) heartUntil = millis() + HEART_MS;
  if (best > memoHi) memoHi = best;
  addBond((uint8_t)(2 + best / 3));
  registerCare();
  save();
  return gain;
}

void Pet::play() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 25);
  energy = clamp100(energy - 10);
  fullness = clamp100(fullness - 5);
  heartUntil = millis() + HEART_MS;
  addBond(2);
  registerCare();
  save();
}

// The hour off the RTC, the same source the scene uses. With no clock at all
// there is no night, so a board that has never been set never auto-sleeps.
bool Pet::isNightHour() const {
  if (!lastSeenEpoch) return false;
  int h = (int)((lastSeenEpoch / 3600) % 24);
  // The window may or may not cross midnight, and it must keep working either
  // way: with NIGHT_START 0 the old `h >= START || h < END` was true for every
  // hour of the day, which put the creature to sleep the moment the screen went
  // off at noon. sleep_test catches it, and did.
  if (NIGHT_START < NIGHT_END) return h >= NIGHT_START && h < NIGHT_END;
  return h >= NIGHT_START || h < NIGHT_END;
}

// Auto-sleep needs the screen off AND the night hours, and is re-checked every
// tick rather than only on the button, so a device put down at 21:00 nods off
// at 22:00 and gets up at 06:00 without anyone touching it.
//
// Both halves earn their place. Screen-off alone paused the game whenever you
// put the device down, and the creature is meant to get hungry during the day.
// The hour alone sent it to bed while you were still playing with it.
//
// Only what this put to sleep is woken by it: a creature the player sent to bed
// with the light button stays there until the player says otherwise.
void Pet::applyAutoSleep() {
  if (isEgg() || ceremony != CER_NONE) return;
  bool night = isNightHour();
  if (screenIsOff && night && !sleeping && sleepAuto != SLEEP_PLAYER) {
    sleeping = true;
    sleepAuto = SLEEP_AUTO;
    pendingSave = true;
  }
  // NOTHING wakes it here, and that is the whole point. Waking at 06:00 would
  // reopen the hole this exists to close: from the sleep floors, food is empty
  // by 06:15 and every stat by 07:40, so anyone who sleeps past eight would
  // find the creature ready to run away again. It sleeps until YOU are up,
  // which is the screen coming back on.
  if (!night && sleepAuto == SLEEP_PLAYER) sleepAuto = SLEEP_NONE;  // a new day
}

void Pet::setScreenOff(bool off) {
  screenIsOff = off;
  // Coming back to the device is what wakes it -- only if the device is what
  // put it to sleep. A creature sent to bed with the light stays there.
  if (!off && sleeping && sleepAuto == SLEEP_AUTO) {
    sleeping = false;
    sleepAuto = SLEEP_NONE;
  }
  applyAutoSleep();
  save();
}

void Pet::toggleLight() {
  if (ceremony != CER_NONE) return;
  if (isEgg()) return;
  sleeping = !sleeping;
  // The player's hand beats the clock until morning: waking it at 23:00 must
  // not be undone a minute later by the auto-sleep, and neither must putting
  // it to bed early.
  sleepAuto = SLEEP_PLAYER;
  save();
}

void Pet::clean() {
  if (ceremony != CER_NONE) return;
  poops = 0;
  hygiene = 100;
  addBond(1);
  registerCare();
  save();
}

void Pet::caress() {
  if (ceremony != CER_NONE) return;
  if (isEgg() || sleeping) return;
  joy = clamp100(joy + 5);
  heartUntil = millis() + HEART_MS;
  addBond(1);
  registerCare();
}

void Pet::eggTap() {
  if (!isEgg()) return;
  if (++eggTaps >= 3) hatch();
  else save();
}

PetMood Pet::mood() const {
  if (sleeping) return MOOD_SLEEPING;
  if (eating()) return MOOD_EATING;
  if (lowestStat() < 25) return MOOD_SAD;
  return MOOD_HAPPY;
}

bool Pet::saveCoreSnapshot() {
  const uint32_t nextGen = saveGeneration + 1;
  PetCoreSnapshot snapshot = {};
  snapshot.ageMinutes = ageMinutes;
  snapshot.lastSeenEpoch = lastSeenEpoch;
  snapshot.formAgeMin = formAgeMin;
  snapshot.speciesId = speciesId;
  snapshot.eggTarget = eggTarget;
  snapshot.medals = medals;
  snapshot.fullness = fullness; snapshot.joy = joy;
  snapshot.energy = energy; snapshot.hygiene = hygiene;
  snapshot.poops = poops; snapshot.weight = weight;
  snapshot.ivAtk = ivAtk; snapshot.ivDef = ivDef;
  snapshot.ivSpe = ivSpe; snapshot.ivHp = ivHp;
  snapshot.trAtk = trAtk; snapshot.trDef = trDef; snapshot.trSpe = trSpe;
  memcpy(snapshot.moves, moves, sizeof(moves));
  memcpy(snapshot.learnQueue, learnQueue, sizeof(learnQueue));
  snapshot.learnCursor = learnCursor; snapshot.learnQCount = learnQCount;
  snapshot.berryKnown = berryKnown; snapshot.variant = variant;
  snapshot.eggVariant = eggVariant; snapshot.starterPick = starterPick;
  snapshot.evoPen = evoPen; snapshot.sleepAuto = sleepAuto;
  snapshot.retirePending = retirePending; snapshot.eggTaps = eggTaps;
  snapshot.careMistakes = careMistakes; snapshot.sleeping = sleeping;
  snapshot.lastEnd = lastEnd; snapshot.frozen = frozen; snapshot.bond = bond;
  snapshot.deDigiCount = deDigiCount; snapshot.evoHold = evoHold;
  strncpy(snapshot.nick, nick, sizeof(snapshot.nick) - 1);

  uint8_t buf[PET_CORE_CAP] = {};
  memcpy(buf, &snapshot, PET_FIXED);

  uint16_t total = ckptSeal(buf, PET_FIXED, PET_CORE_MAGIC, PET_CORE_VERSION, nextGen);
  const char *key = ckptSlot(nextGen, "petA", "petB");
  const size_t writtenBytes = prefs.putBytes(key, buf, total);
  if (writtenBytes != total) {
    Serial.printf("save: %s wrote %u/%u bytes\n", key, (unsigned)writtenBytes, (unsigned)total);
    return false;
  }
  // Read it back and re-check the CRC before believing it. This catches a
  // rejected or short write, which is what a full or failing NVS looks like
  // from up here -- it cannot catch a marginal cell that reads correctly now
  // and decays later, which is what the second slot is for.
  PetCoreSnapshot written;
  if (!readPetCore(prefs, key, written) || written.generation != nextGen) {
    Serial.printf("save: %s read-back failed\n", key);
    return false;
  }
  saveGeneration = nextGen;
  return true;
}

bool Pet::loadCoreSnapshot() {
  uint8_t buf[PET_CORE_CAP];
  uint16_t n = ckptReadNewest(prefs, "petA", "petB", PET_CORE_MAGIC, buf, sizeof(buf));
  if (!n) return false;
  PetCoreSnapshot snapshot;
  petCoreFromBytes(buf, n, snapshot);

  saveGeneration = snapshot.generation;
  ageMinutes = snapshot.ageMinutes;
  lastSeenEpoch = snapshot.lastSeenEpoch;
  formAgeMin = snapshot.formAgeMin;
  speciesId = snapshot.speciesId;
  eggTarget = snapshot.eggTarget;
  medals = snapshot.medals;
  fullness = snapshot.fullness; joy = snapshot.joy;
  energy = snapshot.energy; hygiene = snapshot.hygiene;
  poops = snapshot.poops; weight = snapshot.weight;
  ivAtk = snapshot.ivAtk; ivDef = snapshot.ivDef;
  ivSpe = snapshot.ivSpe; ivHp = snapshot.ivHp;
  trAtk = snapshot.trAtk; trDef = snapshot.trDef; trSpe = snapshot.trSpe;
  memcpy(moves, snapshot.moves, sizeof(moves));
  memcpy(learnQueue, snapshot.learnQueue, sizeof(learnQueue));
  learnCursor = snapshot.learnCursor; learnQCount = snapshot.learnQCount;
  berryKnown = snapshot.berryKnown != 0; variant = snapshot.variant != 0;
  eggVariant = snapshot.eggVariant != 0; starterPick = snapshot.starterPick != 0;
  evoPen = snapshot.evoPen; sleepAuto = snapshot.sleepAuto;
  retirePending = snapshot.retirePending != 0; eggTaps = snapshot.eggTaps;
  careMistakes = snapshot.careMistakes; sleeping = snapshot.sleeping != 0;
  lastEnd = snapshot.lastEnd; frozen = snapshot.frozen != 0; bond = snapshot.bond;
  deDigiCount = snapshot.deDigiCount; evoHold = snapshot.evoHold;
  memcpy(nick, snapshot.nick, sizeof(nick));
  nick[sizeof(nick) - 1] = 0;
  return true;
}

void Pet::save() {
  if (!opened) return;
  // A failure returns before anything else runs, so a rejected write leaves
  // the previous checkpoint exactly as it was rather than half-updating it.
  if (!saveCoreSnapshot()) {
    pendingSave = true;
    if (saveFailures < 255) saveFailures++;
    Serial.println("save: pet checkpoint failed");
    return;
  }
  if (!savePlayerSnapshot()) {
    pendingSave = true;
    if (saveFailures < 255) saveFailures++;
    Serial.println("save: player checkpoint failed");
    return;
  }
  ticksSinceSave = 0;
  pendingSave = false;
  saveFailures = 0;
}

void Pet::load() {
  // The checkpoints are the whole save. A save from before the Time Stranger cutover
  // never gets here: saveReplaceOld() replaced it before this ran.
  const bool playerCkptLoaded = loadPlayerSnapshot();
  if (!playerCkptLoaded && (prefs.isKey("plyA") || prefs.isKey("plyB")))
    Serial.println("save: BOTH player checkpoints failed; starting the player over");
  const bool petCkptLoaded = loadCoreSnapshot();
  // Nothing readable, or a creature no table can draw. There is no other copy to fall
  // back on, so it is a new egg, which is never a misread creature.
  if (!petCkptLoaded || (speciesId != -1 && !dexValid(speciesId))) {
    Serial.println("save: no readable pet checkpoint; starting a new egg");
    speciesId = -1;
    newEgg();
    return;
  }
  // Never index a table with junk.
  for (int i = 0; i < MOVE_SLOTS; i++)
    if (!skillValid(moves[i])) moves[i] = 0;
  const uint8_t queueCap = sizeof(learnQueue) / sizeof(learnQueue[0]);
  if (learnQCount > queueCap) learnQCount = queueCap;
  if (!dexValid(eggTarget)) eggTarget = pickEggSpecies();
  checkLearnGates();
  // seed: the current pet counts as raised
  if (speciesId >= 1) registerSpecies(speciesId);
  // Create the missing record now, once, so the checkpoints are authoritative the
  // moment begin() returns: EXPORT, or a crash, should not see half a save.
  if (!playerCkptLoaded) save();
}

#pragma once
#include <stdint.h>

// The bag's item catalogue.
//
// APPEND-ONLY BELOW THE MARKER. An ItemKey is stored RAW in NVS by every
// inventory stack, exactly like a move index in Pet::moves[] -- inserting a key
// in the middle silently rewrites what every player is carrying. Same family of
// trap as § "uint8_t with a dex of 386" in CLAUDE.md: never reinterpret bytes
// that already exist.
//
// The scan chips left at the Time Stranger cutover, which started every save over,
// so nothing on a device still holds the old keys.
//
// 0 is reserved so an empty stack and the first item cannot be confused.
using ItemKey = uint8_t;
constexpr ItemKey ITEM_NONE = 0;

enum : uint8_t {
  IT_SMRECOVERY = 1,
  IT_MDRECOVERY,
  IT_LGRECOVERY,
  IT_ALLCURE,
  IT_ATKCHIP,
  IT_DEFCHIP,
  IT_SPDCHIP,
  // APPEND-ONLY BELOW HERE
  ITEM_COUNT,
};

enum ItemCategory : uint8_t {
  IC_NONE = 0,   // the empty filler in slot 0
  IC_HEAL,       // restores HP, in battle or out
  IC_CURE,       // clears an ailment
  IC_TRAIN,      // permanently raises one training stat, out of battle
};

// Which training stat an IC_TRAIN item feeds. Matches Pet::rewardTraining()'s
// `which` so the two never disagree about what 0/1/2 mean.
enum : uint8_t { TRAIN_ATK = 0, TRAIN_DEF, TRAIN_SPE };

struct ItemEntry {
  const char *name;
  uint8_t category;
  int16_t param;       // IC_HEAL: HP · IC_TRAIN: stat id
  uint8_t amount;      // IC_TRAIN: how many points
  uint8_t dropWeight;  // 0 = never drops from a wild win
};

// Indexed by ItemKey, so slot 0 is the empty filler and itemEntry() can return a
// reference for any key without a bounds decision at every call site.
static const ItemEntry ITEM_TBL[ITEM_COUNT] = {
  { "-",            IC_NONE,  0,         0, 0 },
  { "SM RECOVERY",  IC_HEAL,  20,        0, 20 },
  { "MD RECOVERY",  IC_HEAL,  50,        0, 10 },
  { "LG RECOVERY",  IC_HEAL,  120,       0, 4 },
  { "ALL CURE",     IC_CURE,  0,         0, 10 },
  { "ATTACK CHIP",  IC_TRAIN, TRAIN_ATK, 5, 8 },
  { "DEFENSE CHIP", IC_TRAIN, TRAIN_DEF, 5, 8 },
  { "SPEED CHIP",   IC_TRAIN, TRAIN_SPE, 5, 8 },
};

inline bool itemValid(ItemKey k) { return k > ITEM_NONE && k < ITEM_COUNT; }
inline const ItemEntry &itemEntry(ItemKey k) {
  return ITEM_TBL[itemValid(k) ? k : ITEM_NONE];
}
// Usable from the bag on the main screen. Only training items: the pet has no
// persistent HP or ailment outside a fight -- those live on the Combatant and
// are discarded when it ends -- so a potion in the field would have nothing to
// act on and would silently vanish.
inline bool itemUsableInField(ItemKey k) {
  return itemValid(k) && itemEntry(k).category == IC_TRAIN;
}
// Usable from inside a fight. Training is permanent and belongs to the care
// sim, so it stays out of the battle bag.
inline bool itemUsableInBattle(ItemKey k) {
  return itemValid(k) && itemEntry(k).category != IC_TRAIN;
}

// The bag and the wild-encounter maths.
//
// Every function under test takes its roll as an argument, so these are pinned
// at the BOUNDARIES rather than at values that happen to hold. CLAUDE.md
// § "A test that proves the transcription rather than the firmware" records
// four bugs found in one session from assertions tied to a value instead of a
// rule -- one of which passed only because 12 % 4 came full circle.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "inventory.h"
#include "wild.h"
#include "noart.h"
#include <cstdio>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=3; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  // ---- the bag -------------------------------------------------------------
  Inventory inv; inv.begin();
  ck(inv.count(IT_SMRECOVERY)==5 && inv.count(IT_ALLCURE)==1,
     "a fresh bag gets the starter kit");
  ck(inv.count(ITEM_NONE)==0, "the filler entry is never carried");

  inv.add(IT_LGRECOVERY, 2);
  ck(inv.count(IT_LGRECOVERY)==2, "adding a new item stacks it");
  ck(inv.consume(IT_LGRECOVERY, 2) && inv.count(IT_LGRECOVERY)==0,
     "consuming the last of a stack empties it");
  ck(!inv.consume(IT_LGRECOVERY, 1), "and an empty stack cannot be consumed");
  ck(!inv.consume(IT_SMRECOVERY, 99), "nor can more than is carried");

  // The cap has to CLIP rather than wrap: counts are uint8_t, and 99 + 200
  // overflowing to 43 would silently destroy a player's supply.
  ck(inv.add(IT_SMRECOVERY, 200) == BAG_STACK_MAX - 5,
     "a stack fills to the cap and reports what it took");
  ck(inv.count(IT_SMRECOVERY)==BAG_STACK_MAX, "and stops exactly there");
  ck(inv.add(IT_SMRECOVERY, 10)==0, "a full stack takes nothing more");

  // Growing ITEM_TBL must be additive, exactly like the Digidex bitmaps: a blob
  // written before an item existed has to leave every earlier count in place.
  { Preferences seed; seed.begin("tamadigi", false);
    uint8_t oldBag[4] = { 0, 7, 0, 2 };   // a build that only knew 3 items
    seed.putBytes("bag", oldBag, sizeof(oldBag));
    seed.end(); }
  Inventory grown; grown.begin();
  ck(grown.count(IT_SMRECOVERY)==7 && grown.count(IT_LGRECOVERY)==2,
     "a bag saved by a build with fewer items keeps every count");
  ck(grown.count(IT_SPDCHIP)==0, "and the items it never knew read as zero");
  ck(grown.count(IT_ALLCURE)==0, "no starter kit is re-granted over a real save");

  // ---- the bag holds what Time Stranger's rules leave in it
  {
    int heal = 0, cure = 0, train = 0, other = 0;
    for (ItemKey k = 1; k < ITEM_COUNT; k++)
      switch (itemEntry(k).category) {
        case IC_HEAL: heal++; break;
        case IC_CURE: cure++; break;
        case IC_TRAIN: train++; break;
        default: other++;
      }
    ck(heal == 3 && cure == 1 && train == 3 && other == 0,
       "recovery disks, ALL CURE and stat chips, and no scan chips");
  }

  // ---- weighted drops ------------------------------------------------------
  uint32_t total = grown.weightTotal();
  ck(total == 68, "the drop table weighs what the catalogue says");
  ck(grown.weightedDrop(0)==IT_SMRECOVERY, "roll 0 lands on the first weight");
  ck(grown.weightedDrop(total-1)==IT_SPDCHIP, "and the last roll on the last");
  {
    ItemKey ex[1] = { IT_SMRECOVERY };
    ck(grown.weightTotal(ex,1)==48, "excluding an item removes its weight");
    bool never = true;
    for (uint32_t r = 0; r < 48; r++) if (grown.weightedDrop(r, ex, 1) == IT_SMRECOVERY) never = false;
    ck(never, "and it can no longer be drawn");
  }

  // ---- variant ---------------------------------------------------------------
  ck(wildVariantThreshold(0)==100, "the base variant rate is exactly 1/4096");
  ck(wildVariantThreshold(1)==100+4096, "each bonus point is one percentage point");
  ck(wildVariantThreshold(99)==wildVariantThreshold(WILD_VARIANT_BONUS_MAX),
     "and the bonus is clamped");
  ck(wildVariantForRoll(99,0) && !wildVariantForRoll(100,0),
     "the variant roll flips at its threshold, not near it");

  // ---- level band ----------------------------------------------------------
  ck(wildLevelMin(1,false)==1 && wildLevelMax(1,false)==6,
     "a level 1 player meets 1-6");
  ck(wildLevelMin(50,false)==45 && wildLevelMax(50,false)==55,
     "and a level 50 player meets 45-55");
  ck(wildLevelMax(100,false)==MAX_LEVEL, "the band never exceeds the level cap");
  ck(wildLevelMin(50,true)==1 && wildLevelMax(50,true)==MAX_LEVEL,
     "hard opens the whole ladder");

  // ---- drops per win -------------------------------------------------------
  ck(wildDropCount(false,29)==2 && wildDropCount(false,30)==1,
     "the bonus drop flips exactly at 30%");
  ck(wildDropCount(true,30)==2 && wildDropCount(true,29)==3,
     "hard grants two, plus the same bonus roll");

  // ---- escape --------------------------------------------------------------
  ck(wildEscapeChance(50,50)==90 && wildEscapeChance(50,10)==90,
     "fleeing something at or below your level is 90%");
  ck(wildEscapeChance(50,100)==45, "and scales down by the level ratio above it");
  ck(wildEscapeChance(1,100)==10, "with a 10% floor");
  ck(wildEscapeChance(50,0)==90, "a foe with no level cannot divide by zero");

  // The foe's curve is a TENT, and its shape is the whole design: a nearly
  // beaten creature must stop running. A monotonic curve would pass a naive
  // "low HP flees more" check, so both slopes are pinned.
  ck(wildFoeEscapeChance(100,100)==0, "a healthy foe never runs");
  ck(wildFoeEscapeChance(41,100)==0, "nor does one just above the 40% threshold");
  ck(wildFoeEscapeChance(40,100)==10, "it starts considering it at 40% HP");
  ck(wildFoeEscapeChance(20,100)==20, "peaks at 20% HP");
  ck(wildFoeEscapeChance(10,100)==10, "falls back to 10% at 10% HP");
  ck(wildFoeEscapeChance(1,100)==10, "and stays there below that");
  ck(wildFoeEscapeChance(0,100)==0, "a fainted foe is not running anywhere");

  // ---- the Digimon pool ----------------------------------------------------
  // Nothing art-less may ever be met: it could only draw as a dex number.
  bool artOk=true, stageOk=true;
  for (uint32_t r=0;r<600;r++) {
    uint8_t stage = (uint8_t)(r % STAGE_COUNT);
    int16_t d = wildPickSpecies(stage, r*7919u);
    if (!d) continue;
    if (!speciesHasArt(d)) artOk=false;
    if (TS_DEX_TBL[d].tier != stage) stageOk=false;
  }
  ck(artOk, "no wild encounter is a species with no art");
  ck(stageOk, "and every one is of the stage that was asked for");

  // the stage follows the level: no Champion before a Champion can exist
  ck(wildStageForLevel(1) == TS_ST_IN_TRAINING_1 && wildStageForLevel(7) == TS_ST_ROOKIE &&
     wildStageForLevel(25) == TS_ST_CHAMPION && wildStageForLevel(73) == TS_ST_MEGA_PLUS &&
     wildStageForLevel(100) == TS_ST_MEGA_PLUS,
     "a level's stage is the stage clock's, all seven of them");
  {
    bool early = false;
    for (int r = 0; r < 100; r++) if (wildStageFor(20, (uint8_t)r) > TS_ST_ROOKIE) early = true;
    ck(!early, "a level-20 encounter is never past Rookie");
    bool baby = false;
    for (int r = 0; r < 100; r++) if (wildStageFor(30, (uint8_t)r) == TS_ST_IN_TRAINING_1) baby = true;
    ck(!baby, "and once Rookies exist an In-Training I is never met");
  }

  // Negative check: with no pack there is nothing to meet, and the picker has to say
  // so rather than returning a Digimon anyway. Without this the gating above could
  // pass by never being consulted.
  gArtPack = false;
  ck(wildPickSpecies(TS_ST_ROOKIE, 1)==0, "with no sprite pack installed there is nothing to encounter");
  gArtPack = true;
  ck(wildPickSpecies(TS_ST_ROOKIE, 1)!=0, "and the pool comes back when the pack does");

  printf(bad?"FAILED %d\n":"OK\n", bad);
  return bad?1:0;
}

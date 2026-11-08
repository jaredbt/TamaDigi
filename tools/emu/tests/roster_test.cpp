// The boss ladder as DATA: TS_BOSS_TBL's 13 real bosses, and trainers.h's stand-ins
// for the ones with no public art (tools/gen_ladders.py). The failure modes are a
// level outside the curve, a stand-in of the wrong stage, a fight that cannot be
// drawn -- and a bad dex number, which is an out-of-bounds read into TS_DEX_TBL, not
// a strange-looking opponent. bossForm() is the one answer to what is fought.
#include "Arduino.h"
#include "roster.h"
#include "trainers.h"
#include "pet.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=1; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif
#define MAX_LEVEL_FOR_TEST 100
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  int oob=0, badLvl=0, noName=0, falls=0, noArt=0, dup=0, badStage=0, realKept=0, badKind=0;
  uint8_t prev=0;
  // The lineup from the design table, in order.
  static const char *const LINEUP[TRAINER_COUNT] = { "DEMIDEVIMON", "OGREMON", "UNIMON",
    "DEVIMON", "ETEMON", "MYOTISMON", "MUMMYMON", "ARUKENIMON", "METALSEADRAMON",
    "METALETEMON", "MALOMYOTISMON", "DIABOROMON", "APOCALYMON" };
  ck(TRAINER_COUNT == 13 && TRAINER_GYMS == 8 && TRAINER_ELITE4 == 4 && TRAINER_CHAMPION == 12,
     "13 slots: 8 areas, 4 elites, a champion");
  for (int i=0;i<TRAINER_COUNT;i++){
    const TsBoss &b = TS_BOSS_TBL[i];
    if (!TRAINER_PLACE[i] || !TRAINER_PLACE[i][0]) noName++;
    if (!dexValid((int16_t)b.dex)) { oob++; continue; }
    if (strcmp(TS_DEX_TBL[b.dex].name, LINEUP[i])) {
      printf("      slot %d is %s, the design table says %s\n", i, TS_DEX_TBL[b.dex].name, LINEUP[i]);
      noName++;
    }
    if (b.stage != TS_DEX_TBL[b.dex].stage) badStage++;
    const uint8_t kind = i < TRAINER_GYMS ? TS_BOSS_AREA
                       : (i < TRAINER_GYMS + TRAINER_ELITE4 ? TS_BOSS_ELITE : TS_BOSS_CHAMPION);
    if (b.kind != kind) badKind++;
    if (b.level < 1 || b.level > MAX_LEVEL_FOR_TEST) badLvl++;
    if (b.level < prev) falls++;
    prev = b.level;
    if (bossLevel((uint8_t)i) != b.level) badLvl++;
    // What is fought: the boss itself when it has art, else its stand-in.
    const int16_t f = bossForm((uint8_t)i);
    if (!dexValid(f)) { oob++; continue; }
    if (!speciesHasArt(f)) { printf("      slot %d fights dex %d, which has no sprite\n", i, (int)f); noArt++; }
    if (speciesHasArt((int16_t)b.dex)) {
      if (f != (int16_t)b.dex || TRAINER_STAND_IN[i]) realKept++;
    } else {
      if (f != (int16_t)TRAINER_STAND_IN[i]) realKept++;
      // the same stage, or the strongest below it with art (no Mega+ has public art)
      if (TS_DEX_TBL[f].stage > b.stage) badStage++;
      if (TS_DEX_TBL[f].stage + 1 < b.stage) badStage++;
    }
    for (int j=0;j<i;j++) if (bossForm((uint8_t)j)==f) dup++;
  }
  ck(oob==0, "every boss and every stand-in is a real dex number");
  ck(badLvl==0, "the levels climb DemiDevimon 15 ... Apocalymon 80, and bossLevel() reads them");
  ck(noName==0, "the lineup is the design table's, and every slot is placed");
  ck(badKind==0 && badStage==0, "areas, elites and champion, each of its stage");
  ck(falls==0, "the ladder only climbs");
  ck(dup==0, "and nobody is fought twice");
  ck(realKept==0, "a boss with art is fought as itself; one without, as its stand-in");
  // noart.h and the ladder are generated separately, so nothing else connects them:
  // a boss with no art would put a bare dex number on screen.
  ck(noArt==0, "no boss is fought as a creature with no sprite art");
  ck(!speciesHasArt(1), "and speciesHasArt really does reject an art-less dex");
  ck(TS_BOSS_TBL[TRAINER_COUNT-1].level >= TS_BOSS_TBL[TRAINER_COUNT-2].level, "the champion tops the ladder");
  // Without the pack nothing can be drawn, and the ladder has to say so (0) rather
  // than hand back a form anyway. Negative check for the gate above.
  gArtPack = false;
  ck(bossForm(0) == 0, "with no sprite pack there is no boss to fight");
  gArtPack = true;
  ck(bossForm(0) != 0, "and the pack brings it back");
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

// The boss ladders as DATA. They are generated from LADDERS in
// tools/digimon_data.py (which validates that no boss sits below its stage),
// so the failure mode is a typo that puts a Digimon outside the dex or a level
// outside the curve -- and a bad dex number here is an out-of-bounds read into
// DEX_TBL, not a strange-looking opponent.
//
// This test is the cheap always-on half: it catches the mistakes that would
// crash or unbalance the game.
#include "Arduino.h"
#include "dex.h"
#include "trainers.h"
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
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  // GYM_REGIONS <= REGION_COUNT - 1, NOT equal to it. This asserted equality
  // and so broke the moment Ver.4 landed: the dex can carry a version long
  // before that version has a roster, which is exactly the state the expansion
  // moves through. What must stay true is that no ladder is claimed for the
  // ALL pseudo-version and that every ladder the code indexes really exists --
  // TRAINERS indexes TRAINER_SETS[gymRegion % GYM_REGIONS], so a GYM_REGIONS
  // larger than the table is the crash this guards.
  ck(GYM_REGIONS <= REGION_COUNT - 1,
     "no ladder is claimed for the ALL pseudo-region");
  ck((int)(sizeof(TRAINER_SETS)/sizeof(*TRAINER_SETS)) == GYM_REGIONS,
     "and the ladder table is exactly GYM_REGIONS long");
  {
    int unnamed = 0;
    for (int r = 0; r < GYM_REGIONS; r++)
      if (!TRAINER_SETS[r].list || !TRAINER_SETS[r].region ||
          !TRAINER_SETS[r].region[0]) unnamed++;
    ck(unnamed == 0, "every ladder that exists has a roster and a name");
  }

  for (int r=0; r<GYM_REGIONS; r++){
    const TrainerSet &ts = TRAINER_SETS[r];
    int oob=0, badLvl=0, badCount=0, noName=0;
    uint8_t prevTop=0;
    int outOfRegion=0;
    for (int i=0;i<TRAINER_COUNT;i++){
      const Trainer &t = ts.list[i];
      if (!t.name || !t.name[0] || !t.place || !t.place[0]) noName++;
      if (t.count < 1 || t.count > TRAINER_TEAM_MAX) badCount++;
      uint8_t top=0;
      for (int k=0;k<t.count && k<TRAINER_TEAM_MAX;k++){
        const TrainerMon &m = t.team[k];
        // the one that would actually crash: DEX_TBL has DEX_COUNT+1 entries
        if (m.dex < 1 || m.dex > DEX_COUNT) { oob++; continue; }
        if (m.level < 1 || m.level > MAX_TRAINER_LEVEL) badLvl++;
        if (m.level > top) top = m.level;
      }
      // The ladder is sequential and level-capped to the boss's best, so a
      // later boss being far WEAKER would make the run go backwards. Ver.2's
      // Pryce really is below Jasmine in Gold/Silver, so a small dip is allowed
      // and only a collapse is a failure.
      if (top + 8 < prevTop) outOfRegion++;
      if (top > prevTop) prevTop = top;
    }
    char m[96];
    snprintf(m,sizeof(m),"%s: every creature is a real dex number", ts.region);
    ck(oob==0, m);
    snprintf(m,sizeof(m),"%s: every level is inside the curve", ts.region);
    ck(badLvl==0, m);
    snprintf(m,sizeof(m),"%s: every team has 1..%d members", ts.region, TRAINER_TEAM_MAX);
    ck(badCount==0, m);
    snprintf(m,sizeof(m),"%s: every trainer is named and placed", ts.region);
    ck(noName==0, m);
    snprintf(m,sizeof(m),"%s: the ladder never collapses in difficulty", ts.region);
    ck(outOfRegion==0, m);
  }

  // the champion should be the hardest thing in each ladder
  for (int r=0; r<GYM_REGIONS; r++){
    const TrainerSet &ts = TRAINER_SETS[r];
    uint8_t champ=0, best=0;
    for (int k=0;k<ts.list[TRAINER_COUNT-1].count;k++)
      if (ts.list[TRAINER_COUNT-1].team[k].level > champ)
        champ = ts.list[TRAINER_COUNT-1].team[k].level;
    for (int i=0;i<TRAINER_COUNT-1;i++)
      for (int k=0;k<ts.list[i].count;k++)
        if (ts.list[i].team[k].level > best) best = ts.list[i].team[k].level;
    char m[96];
    snprintf(m,sizeof(m),"%s: the champion tops the ladder (%u vs %u)", ts.region, champ, best);
    ck(champ >= best, m);
  }

  // Some of each version's own generation must appear, which is what would catch
  // a roster pasted into the wrong table. NOT "most": Ver.2's bosses are
  // famously Ver.1-heavy in Gold/Silver -- Falkner's Pidgey, Bugsy's Metapod,
  // Morty's Gastly line, Pryce's Seel, Clair's AGUMON -- so only 13 of its 49
  // are Ver.2 natives, and that is faithful rather than a mistake. Ver.3, which
  // shares almost nothing with Ver.1, comes out at 47 of 57.
  for (int r=1; r<GYM_REGIONS; r++){
    const TrainerSet &ts = TRAINER_SETS[r];
    int own=0, total=0;
    uint16_t lo = REGIONS[r].lo, hi = REGIONS[r].hi;
    for (int i=0;i<TRAINER_COUNT;i++)
      for (int k=0;k<ts.list[i].count;k++){
        total++;
        uint16_t d = ts.list[i].team[k].dex;
        if (d >= lo && d <= hi) own++;
      }
    char m[110];
    snprintf(m,sizeof(m),"%s: %d of %d creatures are its own generation", ts.region, own, total);
    ck(own >= 8, m);              // enough to prove it is the right table
  }

  // ---- no tamer may field a creature that cannot be DRAWN
  //
  // noart.h and the boss ladders are generated separately, so nothing else
  // connects them: a boss slot with no art would put a bare dex number on
  // screen. A future version may bring art-less Digimon, so this has to fail
  // the build rather than wait to be spotted.
  {
    int offenders = 0;
    for (uint8_t r = 0; r < GYM_REGIONS; r++) {
      const TrainerSet &ts = TRAINER_SETS[r];
      for (int i = 0; i < TRAINER_COUNT; i++)
        for (int k = 0; k < ts.list[i].count; k++) {
          int16_t d = (int16_t)ts.list[i].team[k].dex;
          if (!speciesHasArt(d)) {
            printf("      %s %s slot%d: dex %d has no sprite\n",
                   ts.region, ts.list[i].name, k, (int)d);
            offenders++;
          }
        }
    }
    ck(offenders == 0, "no trainer fields a creature with no sprite art");
    // and prove the check can actually fire: NO_ART must not be empty, or the
    // loop above passes by having nothing to test against
    // every Digimon has art today; when one does not, prove the filter bites
    if (NO_ART_COUNT > 0)
      ck(!speciesHasArt(NO_ART[0]), "speciesHasArt really does reject an art-less dex");
    else
      ck(speciesHasArt(1) && speciesHasArt(DEX_COUNT), "every Digimon has sprite art");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

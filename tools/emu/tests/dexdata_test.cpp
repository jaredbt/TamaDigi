// The Digimon TABLE itself, every entry of it, as the firmware reads it.
//
// tools/gen_ts_data.py validates the design rules. This is the other half: the
// failures that are the firmware's, a route pointing past the end of the table, a
// form the engine has no element for, a stat of zero. Each of those is invisible
// until somebody raises that exact Digimon.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "types.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=31; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  printf("      sweeping %d species\n", DEX_COUNT);

  // --- every entry is filled in at all
  {
    int noName = 0, badStage = 0, badAttr = 0, zeroStat = 0, shrinks = 0, badResist = 0, badFlags = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const TsDexEntry &e = TS_DEX_TBL[d];
      if (!e.name || !e.name[0]) noName++;
      if (e.stage >= TS_STAGE_COUNT || e.tier >= TS_RAISABLE_STAGES) badStage++;
      if (e.attr >= TS_ATTR_COUNT) badAttr++;
      for (int s = 0; s < TS_STAT_COUNT; s++) {
        if (!e.base[s] || !e.max[s]) zeroStat++;
        if (e.max[s] < e.base[s]) shrinks++;
      }
      for (int r = 0; r < TS_ELEMENT_COUNT; r++)
        if (e.resist[r] != 0 && e.resist[r] != 5 && e.resist[r] != 10 && e.resist[r] != 15 && e.resist[r] != 20) badResist++;
      if (e.flags != TSF_RAISABLE && e.flags != TSF_WILD_ONLY && e.flags != TSF_FUSION_ONLY) badFlags++;
    }
    ck(noName == 0, "every form has a name");
    ck(badStage == 0, "a stage and a wild tier the engine knows");
    ck(badAttr == 0, "and an attribute");
    ck(zeroStat == 0, "six stats at both ends, none of them zero");
    ck(shrinks == 0, "and no stat shrinks with age");
    ck(badResist == 0, "every resistance is one of 0, 0.5, 1, 1.5 and 2x");
    ck(badFlags == 0, "and every form is exactly one of raisable, wild-only and fusion-only");
  }

  // --- the stat formula does not overflow, at the worst case the save allows
  {
    uint32_t top = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      uint32_t hp = petCalcHp(d, 100, DEDIGI_COUNT_MAX);
      if (hp > top) top = hp;
    }
    printf("      largest HP any form reaches, fully boosted: %u\n", (unsigned)top);
    ck(top > 0 && top < 65535, "HP stays well inside its 16 bits");
  }

  // --- routes point somewhere real, and every one of a form's routes is its own
  {
    int oob = 0, self = 0, wrongFrom = 0, noArtBoss = 0;
    uint32_t walked = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const TsDexEntry &e = TS_DEX_TBL[d];
      for (uint16_t i = e.evoFirst; i < (uint16_t)e.evoFirst + e.evoCount; i++) {
        walked++;
        const TsEvo &r = TS_EVO_TBL[i];
        if ((int16_t)r.from != d) wrongFrom++;
        if (!dexValid((int16_t)r.to)) oob++;
        if (r.to == r.from) self++;
      }
    }
    ck(walked == TS_EVO_COUNT, "evoFirst and evoCount cover every route exactly once");
    ck(oob == 0, "no route points outside the table");
    ck(wrongFrom == 0, "each form's routes are its own");
    ck(self == 0, "and nothing evolves into itself");
  }

  // --- a form with attacks has an element; every form has one the UI can draw
  {
    int noHome = 0, sameFromSignature = 0, fellBack = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const uint8_t el = homeElement(d);
      if (el >= TS_ELEMENT_COUNT) noHome++;
      bool attacks = false;
      const TsDexEntry &e = TS_DEX_TBL[d];
      for (uint8_t i = 0; i < e.learnCount; i++) {
        const TsLearn &l = TS_LEARN_TBL[e.learnFirst + i];
        if (!l.hours && TS_SKILL_TBL[l.skill].cat != TS_SC_SUPPORT) { attacks = true; break; }
      }
      (attacks ? sameFromSignature : fellBack)++;
    }
    printf("      %d forms take their element from a signature, %d from their resistances\n",
           sameFromSignature, fellBack);
    ck(noHome == 0, "every form has a home element");
    ck(sameFromSignature > 400, "and almost all of them from what they are known for");
  }

  // --- every skill is one the engine can run
  {
    int badEl = 0, noHits = 0, badAil = 0, badMask = 0, badAcc = 0;
    for (uint16_t m = 1; m < MOVE_COUNT; m++) {
      const TsSkill &k = TS_SKILL_TBL[m];
      if (k.cat != TS_SC_SUPPORT && k.element >= TS_ELEMENT_COUNT) badEl++;
      if (k.cat != TS_SC_SUPPORT && (!k.hits || !k.power[0])) noHits++;
      if (k.effect == TS_EF_STATUS && (k.param < TS_AIL_PARALYSIS || k.param > TS_AIL_CONFUSION)) badAil++;
      if (k.effect == TS_EF_STAT && !k.statMask) badMask++;
      if (!k.acc || k.acc > 100) badAcc++;
    }
    ck(badEl == 0, "every attack has an element in the table");
    ck(noHits == 0, "and a hit count and power");
    ck(badAil == 0, "every status effect is one of the six");
    ck(badMask == 0, "every stat effect names its stats");
    ck(badAcc == 0, "and every skill can land");
  }

  // --- the game can start: forms with art are chained from the hatchlings to Mega
  {
    bool seen[DEX_COUNT + 1] = { false };
    int16_t q[DEX_COUNT + 1];
    int head = 0, tail = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (TS_DEX_TBL[d].stage == TS_ST_IN_TRAINING_1 && speciesHasArt(d)) { seen[d] = true; q[tail++] = d; }
    ck(tail == 3, "PUNIMON, BOTAMON and POYOMON are the three hatchlings with art");
    while (head < tail) {
      const TsDexEntry &e = TS_DEX_TBL[q[head++]];
      for (uint16_t i = e.evoFirst; i < (uint16_t)e.evoFirst + e.evoCount; i++) {
        int16_t t = (int16_t)TS_EVO_TBL[i].to;
        if (speciesHasArt(t) && !seen[t]) { seen[t] = true; q[tail++] = t; }
      }
    }
    int stage[TS_RAISABLE_STAGES] = { 0 };
    for (int i = 0; i < tail; i++) stage[TS_DEX_TBL[q[i]].stage]++;
    printf("      reachable on art alone: %d forms (%d %d %d %d %d %d %d)\n", tail, stage[0],
           stage[1], stage[2], stage[3], stage[4], stage[5], stage[6]);
    ck(tail >= 30, "a good share of the forms with art can be raised using only forms with art");
    ck(stage[TS_ST_MEGA] > 0, "and that chain reaches Mega");
    ck(NO_ART_COUNT == DEX_COUNT - NO_ART_DRAWABLE && NO_ART_DRAWABLE == 73, "73 forms have art");
    int counted = 0;
    for (int16_t d = 0; d <= DEX_COUNT + 2; d++) if (speciesHasArt(d)) counted++;
    ck(counted == NO_ART_DRAWABLE, "and speciesHasArt() agrees, past both ends of the table too");
  }

  // --- the Digidex bitmap is big enough for the table it indexes
  {
    Pet p; p.begin(); p.factoryReset();
    Pet q; q.begin();
    q.dbgHatchAs(DEX_COUNT, false);      // hatching is what registers a Digimon
    ck(q.isRegistered(DEX_COUNT), "the last form can be registered at all");
    ck(!q.isRegistered(DEX_COUNT - 1), "without spilling into its neighbour");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

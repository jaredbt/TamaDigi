// The Digimon TABLE itself, every entry of it.
//
// Adding a V-Pet version is a data change, and the data is generated -- so the
// way it goes wrong is not a crash but a Digimon that quietly cannot work: a
// branch pointing past the end of the table, an element no attack covers, a
// base stat of zero. Each of those is invisible until somebody raises that
// exact Digimon. This is the sweep that makes the next version safe.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "moves.h"
#include "types.h"
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
    int noName = 0, badType = 0, zeroStat = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &e = DEX_TBL[d];
      if (!e.name || !e.name[0]) noName++;
      if (e.type1 >= TYPE_COUNT) badType++;
      if (e.type2 != T_NONE && e.type2 >= TYPE_COUNT) badType++;
      if (!e.bAtk || !e.bDef || !e.bSpe || !e.bHp || !e.bSpA || !e.bSpD) zeroStat++;
    }
    ck(noName == 0, "every species has a name");
    ck(badType == 0, "and a typing inside the chart");
    ck(zeroStat == 0, "and six base stats, none of them zero");
  }

  // --- digivolutions point somewhere real
  {
    int oob = 0, self = 0, noLevel = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &e = DEX_TBL[d];
      if (!e.evolvesTo) continue;
      if (e.evolvesTo < 1 || e.evolvesTo > DEX_COUNT) { oob++; continue; }
      if (e.evolvesTo == d) self++;
      if (!e.evolveLevel) noLevel++;
    }
    ck(oob == 0, "no evolution points outside the table");
    ck(self == 0, "and nothing evolves into itself");
    ck(noLevel == 0, "and every evolution has a level to reach");
  }

  // --- an digivolution chain must terminate
  {
    int loops = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      int16_t at = d;
      for (int hops = 0; at && hops < 8; hops++) at = DEX_TBL[at].evolvesTo;
      if (at) loops++;                     // still going after 8 hops
    }
    ck(loops == 0, "every evolution chain ends rather than looping");
  }

  static const int16_t NO_ATTACK[] = { 0 };     // every Digimon has one
  static const int16_t NO_LEARNSET[] = { 0 };
  auto known = [](const int16_t *a, size_t n, int16_t d) {
    for (size_t i = 0; i < n; i++) if (a[i] == d) return true;
    return false;
  };

  // --- THE ONE THAT BITES ON A NEW GENERATION: a Digimon with no attack of
  //     its own type. digimon_data.py is 77 hand-picked moves, so a typing that
  //     arrives with a new version can leave creatures with no STAB at all.
  {
    int noStab = 0; int16_t first = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      const DexEntry &e = DEX_TBL[d];
      bool found = false;
      for (uint8_t i = 0; i < learnCount(d) && !found; i++) {
        uint8_t mv = learnMove(d, i);
        if (mv >= MOVE_COUNT) continue;
        const MoveEntry &m = MOVE_TBL[mv];
        if (m.cat == MC_STATUS || !m.power) continue;
        if (m.type == e.type1 || m.type == e.type2) found = true;
      }
      if (found) continue;
      if (known(NO_ATTACK, sizeof(NO_ATTACK)/sizeof(*NO_ATTACK), d)) continue;
      // Name every one of them. Printing only the first sends the next person
      // hunting for the rest by hand, and on a new generation there is never
      // just one -- the whole point of this check is that a new typing arrives
      // faster than digimon_data.py's 77 hand-picked moves cover it.
      printf("      no same-type attack: %s (%d)\n", DEX_TBL[d].name, d);
      noStab++; if (!first) first = d;
    }
    if (noStab) printf("      %d species have no same-type attack, first is %s (%d)\n",
                       noStab, DEX_TBL[first].name, first);
    ck(noStab == 0,
       "every species can learn an attack of its own type, bar the known ones");
  }

  // --- and everything it can learn is a real move
  {
    int oob = 0, empty = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      if (!learnCount(d)) {
        if (!known(NO_LEARNSET, sizeof(NO_LEARNSET)/sizeof(*NO_LEARNSET), d)) empty++;
        continue;
      }
      for (uint8_t i = 0; i < learnCount(d); i++)
        if (learnMove(d, i) >= MOVE_COUNT) oob++;
    }
    ck(oob == 0, "no learnset entry points past the move table");
    ck(empty == 0, "and no unexpected species has an empty learnset");
  }

  // --- the versions tile the dex with no gaps and no overlaps
  {
    int uncovered = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      int in = 0;
      for (uint8_t r = 0; r < REGION_COUNT; r++) {
        if (r == REGION_ALL) continue;
        if (d >= REGIONS[r].lo && d <= REGIONS[r].hi) in++;
      }
      if (in != 1) uncovered++;
    }
    ck(uncovered == 0, "every species belongs to exactly one region");
    ck(REGIONS[REGION_ALL].hi == DEX_COUNT, "and ALL reaches the end of the table");
  }

  // --- the Digidex bitmap is big enough for the table it indexes
  {
    Pet p; p.begin(); p.factoryReset();
    Pet q; q.begin();
    q.dbgHatchAs(DEX_COUNT, false);      // hatching is what registers a Digimon
    ck(q.isRegistered(DEX_COUNT), "the last species can be registered at all");
    ck(!q.isRegistered(DEX_COUNT - 1), "without spilling into its neighbour");
  }

  // --- an digivolution TARGET must never hatch from an egg, or the same creature
  //     arrives two ways and the chain stops meaning anything. gen_dex.py
  //     derives R_EVO from being somebody's target, so this locks that
  //     derivation rather than restating it.
  {
    // EVERY branch in EVO_TBL, not just the first one DexEntry carries
    bool isTarget[DEX_COUNT + 1] = { false };
    for (int i = 0; i < EVO_COUNT; i++)
      if (EVO_TBL[i].to >= 1 && EVO_TBL[i].to <= DEX_COUNT)
        isTarget[EVO_TBL[i].to] = true;

    int hatchable = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++)
      if (isTarget[d] && DEX_TBL[d].rarity != R_EVO) {
        if (hatchable < 3) printf("      %s (%d) is an evolution AND hatches\n",
                                  DEX_TBL[d].name, d);
        hatchable++;
      }
    ck(hatchable == 0, "no evolution target also hatches straight from an egg");

    // and the reverse: R_EVO means "only ever reached by digivolving", so a
    // Digimon no branch leads to would be unreachable entirely.
    int stranded = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      if (DEX_TBL[d].rarity != R_EVO || isTarget[d]) continue;
      printf("      %s (%d) can be neither hatched nor digivolved into\n",
             DEX_TBL[d].name, d);
      stranded++;
    }
    ck(stranded == 0, "and every evolution-only species is reachable by evolving");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

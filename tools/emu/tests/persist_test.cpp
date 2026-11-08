// Player-wide progress must outlive the creature that earned it. Badges, the
// avatar, the streak, the Digidex (seen, scan progress, raised) and the partner
// history all belong to the player, not the pet, so none of the three endings may
// take them -- and each ending ADDS to the history.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>
#include <cstring>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=9; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
static uint32_t g_ms=0; uint32_t millis(){return g_ms;}
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static void award(Pet &p){
  p.winBadge(0,false); p.winBadge(1,false); p.winBadge(0,true);
  p.winBadge(12,false);    // and the champion, past the eight area badges
  p.avatar = 2; p.streak = 9; p.bestStreak = 11; p.totalMedals = 5;
  p.markSeen(300);                     // seen in the wild
  if (!p.scanProgress(301)) p.addScan(301, 2);   // two wild wins (award() runs repeatedly)
  p.addScan(DEX_COUNT, SCAN_FULL);     // a boss win, on the roster's last bit
  p.saveNow();
}
static bool intact(Pet &p){
  return p.hasBadge(0,false) && p.hasBadge(1,false) && p.hasBadge(0,true)
      && p.hasBadge(12,false)
      && p.avatar==2 && p.streak==9 && p.bestStreak==11 && p.totalMedals==5
      && p.dexState(300)==DEX_SEEN && p.scanProgress(300)==0
      && p.dexState(301)==DEX_SEEN && p.scanProgress(301)==2
      && p.dexState(DEX_COUNT)==DEX_SCANNED && p.dexState(299)==DEX_UNSEEN;
}

int main(){
  Pet p; p.begin();
  p.dbgSetEgg(6);
  if (p.isEgg()) p.dbgHatchAs(4,false);
  award(p);
  ck(intact(p), "progress is set");

  // 1. a new egg (what every ending eventually calls)
  p.newEgg();
  ck(intact(p), "survives newEgg()");

  // 2. a reload from NVS
  Pet q; q.begin();
  ck(intact(q), "survives a save/load round trip");

  // 3. each of the three endings, end to end
  const char *names[] = {"farewell","runaway","release"};
  const int16_t forms[] = { 89, 86, 21 };        // GREYMON, GARURUMON, AGUMON
  const uint8_t histBefore = q.historyCount;
  for (int e=0;e<3;e++){
    Pet r; r.begin();
    r.dbgHatchAs(forms[e], e == 1);
    award(r);
    r.rename(e == 2 ? "SPARKY" : "");
    r.ageMinutes = 4UL*24*60 + (uint32_t)e * 60;
    if (e==0) r.startFarewell(); else if (e==1) r.startRunaway(); else r.release();
  #ifdef TAMA_NATIVE_TEST
    delay(CEREMONY_MS + 1000);
    r.update(millis());
  #else
    g_ms += 60000;                       // let the ceremony expire
    r.update(g_ms);
  #endif
    ck(r.isEgg(), "the ending actually completes before checking persistence");
    char msg[64]; snprintf(msg,sizeof(msg),"survives a %s",names[e]);
    ck(intact(r), msg);
    Pet after; after.begin();
    snprintf(msg,sizeof(msg),"...and is still there after reloading (%s)",names[e]);
    ck(intact(after), msg);
    snprintf(msg,sizeof(msg),"...and the %s is the newest partner in the history",names[e]);
    ck(after.historyCount == histBefore + e + 1 && after.history[0].dex == forms[e] &&
       after.history[0].end == (e == 0 ? PART_FAREWELL : e == 1 ? PART_RUNAWAY : PART_RELEASE) &&
       after.history[0].ageHours == 96 + e, msg);
    ck(after.isRegistered(forms[e]), "the partner that left is marked raised");
  }
  {
    Pet h; h.begin();
    ck(!strcmp(h.history[0].name, "SPARKY") && !h.history[1].name[0],
       "the history keeps a nickname, and an empty one for an unnamed partner");
    ck((h.history[1].flags & PARTNER_VARIANT) && !(h.history[0].flags & PARTNER_VARIANT),
       "and which partner was a colour variant");
    ck(h.history[2].dex == forms[0] && h.history[1].dex == forms[1], "newest first");
  }
  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

// The ladder's difficulty curve, measured rather than assumed.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "trainers.h"
#include "roster.h"
#include <cstdio>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed = 999;
FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0, g_touchY=0; volatile bool g_touchDown=false;
bool wasPressed=false;
uint32_t millis(){return 0;}
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif

static void foeOf(Combatant &c, int16_t dex, uint8_t lvl) {
  Pet f; f.dbgHatchAs(dex,false);
  f.ageMinutes=(uint32_t)(lvl?lvl-1:0)*MINUTES_PER_LEVEL;
  f.relearnFromLevel(); combatantFromPet(c,f);
}
// one 1v1, returns true if `you` survived
static bool duel(Combatant &you, Combatant &foe) {
  TurnLog lg; int t=0;
  while(!you.fainted() && !foe.fainted() && t<300){
    t++;
    uint16_t my=aiChooseMove(you,foe,true), mf=aiChooseMove(foe,you,false);
    Combatant *a=&you,*b=&foe; uint16_t ma=my,mb=mf;
    if(!battleMovesFirst(you,my,foe,mf)){a=&foe;b=&you;ma=mf;mb=my;}
    battleAct(*a,*b,ma,lg);
    if(!b->fainted()) battleAct(*b,*a,mb,lg);
    battleEndTurn(you,lg); battleEndTurn(foe,lg);
  }
  return !you.fainted();
}
int main(){
  printf("%-16s %s\n","BOSS","solo win-rate for a GREYMON at the boss's level, untrained / trained");
  for (int ti=0; ti<TRAINER_COUNT; ti++){
    const Trainer &t=TRAINERS[ti];
    int wins[2]={0,0}, runs=40;
    for (int trained=0; trained<2; trained++)
      for(int r=0;r<runs;r++){
        Pet p; p.dbgHatchAs(89,false);
        p.ageMinutes=(uint32_t)(t.level-1)*MINUTES_PER_LEVEL;
        if (trained) p.trAtk=p.trDef=p.trSpe=(uint8_t)(t.level<100?t.level:100);
        p.relearnFromLevel();
        Combatant you; combatantFromPet(you,p,t.level);
        Combatant foe; foeOf(foe,(int16_t)t.dex,t.level);
        if(duel(you,foe)) wins[trained]++;
      }
    printf("  %-16s L%-3u %3d%% / %3d%%\n", t.name, t.level, wins[0]*100/runs, wins[1]*100/runs);
  }
  return 0;
}

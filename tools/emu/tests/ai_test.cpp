// Does the hard-mode AI actually play better? A "smart" chooser that does not
// beat random is not smart, so this measures it head to head rather than
// asserting the code merely runs.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include "trainers.h"
#include "roster.h"
#include "types.h"
#include <cstdio>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=4242; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

static void mk(Combatant &c,int16_t dex,uint8_t lvl){
  Pet p; p.dbgHatchAs(dex,false);
  p.ivAtk=p.ivDef=p.ivSpe=p.ivHp=31;
  p.ageMinutes=(uint32_t)(lvl-1)*MINUTES_PER_LEVEL;
  p.relearnFromLevel(); combatantFromPet(c,p);
}
// returns true if side A won
static bool duel(int16_t da,int16_t db,uint8_t lvl,bool aSmart,bool bSmart){
  Combatant A,B; mk(A,da,lvl); mk(B,db,lvl);
  TurnLog lg; int t=0;
  while(!A.fainted()&&!B.fainted()&&t<300){
    t++;
    uint16_t ma=aiChooseMove(A,B,aSmart), mb=aiChooseMove(B,A,bSmart);
    Combatant *f=&A,*s=&B; uint16_t mf=ma,ms=mb;
    if(!battleMovesFirst(A,ma,B,mb)){f=&B;s=&A;mf=mb;ms=ma;}
    battleAct(*f,*s,mf,lg);
    if(!s->fainted()) battleAct(*s,*f,ms,lg);
    battleEndTurn(A,lg); battleEndTurn(B,lg);
  }
  return !A.fainted();
}
int main(){
  // mirror matches: same Digimon both sides, so only the choosing differs
  // GREYMON, GARURUMON, ANGEMON, KABUTERIMON, DEVIMON, OGREMON, UNIMON, BIRDRAMON
  const int16_t roster[]={89,86,83,84,155,140,105,98};
  int smartWins=0,total=0;
  for(int i=0;i<8;i++)
    for(int r=0;r<25;r++){
      if(duel(roster[i],roster[i],50,true,false)) smartWins++;
      total++;
    }
  int pct=smartWins*100/total;
  printf("     mirror matches, smart vs random: %d/%d = %d%%\n",smartWins,total,pct);
  ck(pct>60,"the hard AI beats the random one in a mirror match");

  // it must take a kill when one is available
  Combatant me,foe; mk(me,89,50); mk(foe,86,50);
  foe.hp=6;                         // one hit from fainting
  uint16_t pick=aiChooseMove(me,foe,true);
  uint16_t dmg=battleDamage(me,foe,pick,false,236);
  printf("     foe at 6hp: picks %s for ~%u\n",moveName(pick),dmg);
  ck(dmg>=foe.hp,"it takes the kill when one is on the table");

  // it must not throw a skill the target is immune to, and Time Stranger's table
  // really has immunities: find a Digimon and the element it takes nothing from
  int16_t ghostDex=0; uint8_t nullEl=0;
  for(int16_t d=1;d<=DEX_COUNT&&!ghostDex;d++)
    for(uint8_t e=0;e<TS_ELEMENT_COUNT;e++)
      if(TS_DEX_TBL[d].resist[e]==0){ghostDex=d;nullEl=e;break;}
  ck(ghostDex!=0,"the table has an immunity to test against");
  uint16_t wasted=0,useful=0;
  for(uint16_t m=1;m<=22;m++){            // the 22 learnable attacks, one per element and kind
    if(TS_SKILL_TBL[m].element==nullEl) wasted=m;
    else if(TS_DEX_TBL[ghostDex].resist[TS_SKILL_TBL[m].element]>=10) useful=m;
  }
  Combatant norm,ghost; mk(norm,89,50); mk(ghost,ghostDex,50);
  norm.moves[0]=wasted; norm.moves[1]=useful; norm.moves[2]=0; norm.moves[3]=0;
  bool everImmune=false;
  for(int i=0;i<40;i++) if(aiChooseMove(norm,ghost,true)==wasted) everImmune=true;
  ck(!everImmune,"it never picks a skill the target is immune to");

  // and easy mode must stay dumb, or hard mode means nothing
  Combatant e1,e2; mk(e1,89,50); mk(e2,86,50);
  e1.moves[0]=1; e1.moves[1]=3; e1.moves[2]=15; e1.moves[3]=21;
  bool varied=false; uint16_t first=aiChooseMove(e1,e2,false);
  for(int i=0;i<40 && !varied;i++) if(aiChooseMove(e1,e2,false)!=first) varied=true;
  ck(varied,"easy mode still picks at random");

  // a Digimon with nothing but support skills still attacks, or a fight never ends
  Combatant s1,s2; mk(s1,89,50); mk(s2,86,50);
  s1.moves[0]=23; s1.moves[1]=25; s1.moves[2]=0; s1.moves[3]=0;
  bool hits=false;
  for(int i=0;i<40;i++) if(aiChooseMove(s1,s2,false)==SKILL_BASIC) hits=true;
  ck(hits,"a support-only Digimon falls back on the basic attack");
  Combatant nothing; nothing.dex=89;
  ck(aiChooseMove(nothing,s2,true)==SKILL_BASIC && aiChooseMove(nothing,s2,false)==SKILL_BASIC,
     "and one with no skills at all never comes back with 0");

  printf("%s\n",bad?"FAILURES":"all good");
  return bad?1:0;
}

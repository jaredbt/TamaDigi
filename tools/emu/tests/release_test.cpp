// Where the 3 s hold that offers to release the live pet is allowed to fire.
//
// The release is IRREVERSIBLE and reached by a hold on a round panel, which is
// the exact shape CLAUDE.md section 4 warns about. This file used to pin the
// party and box sheets' RELEASE as well (a confirm that arms on the first tap,
// NO that really cancels, a modal that swallows the taps under it); those
// sheets went with the party and box, removed by design -- one partner.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
uint32_t g_seed=1234; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void loop(); void render();
uint8_t uiCurrentScreen();
extern const char *const SCREEN_NAME[];
extern uint32_t confirmUntil;
extern uint8_t dimStage;
extern bool holdFired;
extern Pet pet;
extern bool playerOpen;

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}


// handleTouch() self-gates to 50 Hz off millis(), so real time has to pass
// between polls; a tight spin is swallowed by that gate.
static void pump(int n){ for(int i=0;i<n;i++){ std::this_thread::sleep_for(std::chrono::milliseconds(30)); loop(); } }
static void tap(int x,int y){
  g_touchX=x; g_touchY=y; g_touchDown=true; emuFireInterrupt();
  pump(5);
  g_touchDown=false; emuFireInterrupt(); pump(3);
}
static void hold(int x,int y,int ms){
  g_touchX=x; g_touchY=y; g_touchDown=true; emuFireInterrupt();
  pump(ms/30);
  g_touchDown=false; emuFireInterrupt(); pump(3);
}

static void clearAll(){ playerOpen=false; }

int main(){
  setup();
  for(int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(4);
  if (pet.isEgg()) pet.dbgHatchAs(6,false);
  pet.ageMinutes = 60UL*40;
  while (pet.hasLearnOffer()) pet.declineLearn();
  // ---- the 3 s hold, and WHERE it is allowed to fire
  //
  // The hold opens "release the live pet?" -- unconfirmed, irreversible, and
  // drawn over the creature. It used to be gated by a hand-kept list of screens
  // to EXCLUDE (gallery, card, keyboard, clock), which left it live on the party
  // screen, whose grid overlapped inPetZone -- and on every other screen added
  // since. The party screen is gone; the player card, another tile drawn over
  // the same zone, stands in for "any screen that is not the main one".
  //
  // BOTH halves are asserted. "It never fires off the main screen" is vacuously
  // true if the hold has stopped working everywhere, which would delete a real
  // feature and still pass -- and an earlier version of this check did exactly
  // that, silently, until the old gate was put back and it stayed green.
  {
    clearAll();
    if (pet.isEgg()) pet.dbgHatchAs(6,false);
    while (pet.hasLearnOffer()) pet.declineLearn();
    if (pet.sleeping) pet.toggleLight();

    // Wake first: swallowGesture is (dimStage > 0 || screenOff), sampled when the
    // finger goes down, so a dimmed panel eats the hold before the gate is even
    // consulted. (440,440) is off every control on both screens.
    playerOpen=true;
    tap(440,440);
    playerOpen=true;                        // a stray tap closes the card; reopen it
    ck(dimStage==0, "the panel is awake, so the gate is what is being tested");
    ck(!strcmp(SCREEN_NAME[uiCurrentScreen()],"player"), "and the player card is up");
    confirmUntil=0; holdFired=false;
    hold(233,200,3400);                     // where the creature is on the main screen
    ck(confirmUntil==0, "holding on another screen does NOT open the release dialog");

    clearAll();
    tap(440,440);
    ck(dimStage==0, "still awake for the companion check");
    ck(!strcmp(SCREEN_NAME[uiCurrentScreen()],"main"),
       "and back on the main screen, or the check below proves nothing");
    confirmUntil=0; holdFired=false;
    hold(233,200,3400);                     // the creature itself
    ck(confirmUntil!=0, "the hold STILL works on the main screen, where it belongs");
    confirmUntil=0;
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

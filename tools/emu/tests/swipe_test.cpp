// Every paged screen must PAGE rather than closing, and the horizontal axis
// must never close anything at all.
//
// This has been got wrong four separate times -- the move picker, the player
// card, the boss list and the (since removed) box -- each found by hand. Paging now lives on the
// RIM (onRim) and the horizontal swipe is the TILE AXIS, which is what makes
// the two gestures impossible to confuse; both halves are checked here.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "noart.h"
#include "items.h"
#include "inventory.h"
#include <cstdio>
uint32_t g_seed=2; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}
void setup(); void render(); void onSwipe(int dir); void onSwipeV(int dir);
void onTap(int16_t x, int16_t y);
void onRim(int dir);
int uiTileIndex(); void uiTileGo(int i);
uint8_t uiCurrentScreen(); uint8_t uiRimPages(); uint8_t uiRimPage();
extern Pet pet;
extern bool cardOpen, galleryOpen, clockOpen, kbOpen, menuOpen;
extern bool trainOpen, movePickOpen, battleOpen, gymOpen, playerOpen;
extern bool exploreOpen;
extern bool btlWild;
extern uint8_t cardPage, gymPage, playerPage, movePickPage;
extern uint8_t galleryPage; extern bool galleryDirty; extern uint8_t galleryDetail;
extern uint8_t movePickSlot;
extern bool bagOpen; extern uint8_t bagPage;
static int bad=0;
static void clearAll(){
  cardOpen=galleryOpen=clockOpen=kbOpen=menuOpen=false;
  trainOpen=movePickOpen=battleOpen=gymOpen=playerOpen=false;
  exploreOpen=false;
  bagOpen=false;
}
// Page it forward; the page must advance and the screen must stay open. Paging
// moved from the horizontal swipe to the RIM, which is what freed the
// horizontal axis to mean one thing. The bug this guards is unchanged: a paged
// screen that CLOSES instead of paging.
static void check(const char *name, bool *open, uint8_t *page){
  *page = 0;
  onRim(1);
  if (!*open) { printf("FAIL  %-10s closed while paging\n", name); bad++; return; }
  if (*page != 1) { printf("FAIL  %-10s did not advance a page (page=%u)\n", name, *page); bad++; return; }
  // and it must CLAMP at the far end rather than closing or wrapping
  uint8_t n = uiRimPages();
  for (int i = 0; i < n + 3; i++) onRim(1);
  if (!*open) { printf("FAIL  %-10s closed when paged off the end\n", name); bad++; return; }
  if (*page != n - 1) { printf("FAIL  %-10s did not clamp at the last page (%u of %u)\n", name, *page, n); bad++; return; }
  printf("PASS  %-10s pages on the rim and clamps at both ends\n", name);
}
uint8_t learnableFor(int16_t dex, uint8_t hours, uint16_t *out, uint8_t max);
uint8_t learnableList(uint16_t *out, uint8_t max);

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(6);
  pet.dbgHatchAs(377,false);           // DARKDRAMON: enough skills for a second page
  pet.ageMinutes = 60UL*60; pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();

  clearAll(); cardOpen=true;                      check("card",     &cardOpen,     &cardPage);
  clearAll(); gymOpen=true;                       check("gyms",     &gymOpen,      &gymPage);
  clearAll(); playerOpen=true;                    check("player",   &playerOpen,   &playerPage);
  clearAll(); movePickOpen=true; movePickSlot=0;
                                                  check("movepick", &movePickOpen, &movePickPage);
  // The bag is paged too. It needs enough DISTINCT items to reach a second page
  // -- with one page it would close on the swipe and the check would fail for
  // the right reason, which is not the same as passing.
  clearAll();
  for (ItemKey k=1;k<ITEM_COUNT;k++) bag.add(k, 1);
  bagOpen=true;                                   check("bag",      &bagOpen,      &bagPage);
  // The Digidex pages to the end of the forms with art. It was capped at 10 flat
  // pages when the dex was 151 long, which silently hid everything past 160 once it
  // grew to 386.
  clearAll(); galleryOpen=true; galleryDetail=0; galleryPage=0;
  onRim(1);
  if (!galleryOpen) { printf("FAIL  gallery    closed while paging\n"); bad++; }
  else if (galleryPage != 1) { printf("FAIL  gallery    did not advance (page=%u)\n", galleryPage); bad++; }
  else printf("PASS  %-10s pages on the rim\n", "gallery");
  {
    const int pages = (NO_ART_DRAWABLE + 15) / 16;
    galleryPage = 0;
    for (int p = 0; p < pages + 2; p++) onRim(1);
    if (galleryPage != pages - 1) {
      printf("FAIL  gallery    stops at page %d of %d\n", galleryPage + 1, pages); bad++;
    } else printf("PASS  %-10s every one of the %d forms with art is reachable\n", "gallery", NO_ART_DRAWABLE);
  }

  // THE TILE AXIS. Horizontal is now one gesture with one meaning: move to the
  // neighbouring tile. It must BUMP at both ends rather than closing, because
  // "page off the end to exit" -- one gesture meaning two things -- is the
  // shape that produced the four paging bugs above.
  {
    clearAll();
    int home = uiTileIndex();
    if (home < 0) { printf("FAIL  tiles      the main screen is not on the axis\n"); bad++; }
    else printf("PASS  %-10s the main screen is tile %d\n", "tiles", home);

    // walk to the far left end and keep going
    for (int i = 0; i < 8; i++) onSwipe(1);
    int left = uiTileIndex();
    if (left != 0) { printf("FAIL  tiles      swiping right does not reach tile 0 (got %d)\n", left); bad++; }
    else printf("PASS  %-10s reaches the first tile and stops there\n", "tiles");

    // ...and the far right
    for (int i = 0; i < 12; i++) onSwipe(-1);
    int right = uiTileIndex();
    if (right < 0) { printf("FAIL  tiles      swiping off the right end left the axis entirely\n"); bad++; }
    else if (right == left) { printf("FAIL  tiles      the axis wrapped instead of bumping\n"); bad++; }
    else printf("PASS  %-10s reaches the last tile (%d) and bumps\n", "tiles", right);

    // every tile must be reachable, and each must be a DIFFERENT screen
    for (int i = 0; i < 20; i++) onSwipe(1);
    uint8_t scr[8]; int n = 0;
    for (int i = 0; i < right + 1 && n < 8; i++) {
      scr[n++] = uiCurrentScreen();
      onSwipe(-1);
    }
    int dup = 0;
    for (int a = 0; a < n; a++)
      for (int b = a + 1; b < n; b++) if (scr[a] == scr[b]) dup++;
    if (dup) { printf("FAIL  tiles      two tiles show the same screen\n"); bad++; }
    else printf("PASS  %-10s all %d tiles are distinct screens\n", "tiles", n);

    // and DOWN is back: from any tile it returns to the pet
    uiTileGo(0);
    onSwipeV(1);
    if (uiCurrentScreen() != scr[home]) {
      // scr[] was captured left-to-right from tile 0, so the pet is at `home`
      printf("FAIL  tiles      swipe down from a tile does not return home\n"); bad++;
    } else printf("PASS  %-10s swipe down returns to the pet\n", "tiles");
  }

  // Each tile opens on its own screen, and a swipe down from it goes home.
  {
    clearAll();
    onSwipe(-1);                       // the first world-facing stop is Explore
    if (!exploreOpen) { printf("FAIL  explore    is not beside the main screen\n"); bad++; }
    else printf("PASS  %-10s is beside the main screen\n", "explore");
    onTap(233, 334);                    // the large Explore action
    if (!battleOpen || !btlWild || exploreOpen) {
      printf("FAIL  explore    its primary action does not start a wild battle\n"); bad++;
    } else printf("PASS  %-10s starts a wild battle from its primary action\n", "explore");
    clearAll();
    onSwipe(-1);
    onSwipe(-1);                       // Bosses follows Explore
    if (!gymOpen) { printf("FAIL  gyms       is not the stop after Explore\n"); bad++; }
    else printf("PASS  %-10s is the stop after Explore\n", "gyms");
    onSwipeV(1);
    if (gymOpen) { printf("FAIL  gyms       swiping down does not go home\n"); bad++; }
    else printf("PASS  %-10s swiping down goes home\n", "gyms");
    clearAll(); galleryOpen = true; galleryPage = 0;
    onSwipeV(1);
    if (galleryOpen) { printf("FAIL  gallery    swiping down does not go home\n"); bad++; }
    else printf("PASS  %-10s swiping down goes home\n", "gallery");
  }

  // The hour offsets live in the learnset and moves_test pins them -- but the
  // PICKER has to actually ask. It once kept its own copy of the check, which IS the
  // bug, so this drives the screen's own list, not the rule.
  {
    const int16_t GREYMON = 89;
    uint16_t l[64];
    const uint8_t special = learnableFor(GREYMON, 0, l, 64);
    bool flame = false;
    for (uint8_t i = 0; i < special; i++) if (!strcmp(moveName(l[i]), "MEGA FLAME")) flame = true;
    if (!flame) { printf("FAIL  picker     lost the special a form has at once\n"); bad++; }
    else printf("PASS  picker     a form offers its special at hour 0\n");
    if (learnableFor(GREYMON, 99, l, 64) <= special) {
      printf("FAIL  picker     later skills never arrive at all\n"); bad++;
    } else printf("PASS  picker     and later skills arrive with the hours\n");
    // the screen's own list: the slots, then what the form offers
    pet.dbgHatchAs(GREYMON, false);
    pet.moves[3] = 1;                  // a skill carried over from another form
    uint16_t all[64];
    const uint8_t n = learnableList(all, 64);
    bool carried = false, offered = false;
    for (uint8_t i = 0; i < n; i++) {
      if (all[i] == 1) carried = true;
      if (!strcmp(moveName(all[i]), "MEGA FLAME")) offered = true;
    }
    if (!carried || !offered) { printf("FAIL  picker     does not list both the slots and the form\n"); bad++; }
    else printf("PASS  picker     lists a carried-over slot beside what the form offers\n");
  }

  printf("%s\n", bad?"FAILURES":"every paged screen pages");
  return bad?1:0;
}

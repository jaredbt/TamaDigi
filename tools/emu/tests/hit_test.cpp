// Where the battle grid can actually be TAPPED, as opposed to where it is drawn.
//
// This exists because of a hardware report: the two bottom buttons were much
// harder to press than the top two. The emulator could never have found it --
// its taps are exact coordinates, so a cell that is one pixel tall still passes
// every synthetic test. A finger is not exact, and the drawn cells are 44 px
// tall with an 8 px dead gap between the rows and nothing live beneath them.
//
// So the thing worth asserting is not "a tap in the middle works", it is that
// there are NO DEAD PIXELS between the cells and that the bottom row is not
// smaller than the top row.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "battle.h"
#include <cstdio>
#include <cmath>
uint32_t g_seed=4; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;}
String FakeSerial::readStringUntil(char){return String("");}

void setup(); void render(); void battleTap(int16_t,int16_t);
extern Pet pet;
extern bool battleOpen;
extern uint8_t choiceKind;
extern uint8_t btlMenu;
extern Combatant btlYou;
void startBattle(int16_t dex, uint8_t lvl);
int btlCellIndexAt(int16_t x, int16_t y);
void uiButtonHeights(int *out, int max, int *n);
void uiRowHeights(int *out, int max, int *n);
void gymHeaderRects(int *pillTop, int *pillBot, int *rowTop);
int uiSleepButton(int *cx, int *cy);
bool uiButtonDisabled(int i);
void uiButtonAt(int i, int *cx, int *cy, int *half);
void uiConfirmRects(int *b1Top, int *b1Bot, int *b2Top, int *b2Bot);
// The round-panel geometry, asked of the firmware rather than restated here.
int uiSafeHalfWidth(int y);
int uiTapFinger();
int uiButtonHitR();
// the memory game and the training menu, asked of the firmware not restated
int uiMemoPads();
int uiMemoPadR();
void memoPadPos(int i, int *cx, int *cy);
int memoPadAt(int16_t x, int16_t y);
int uiTrainRows();
int uiTrainRowH();
int uiTrainRowTop(int i);
int uiTrainPanelTop();
int uiTrainPanelBot();
int uiTrainPanelHalfW();
int uiCareArcTop(int i);
int uiHeaderNameBottom();
#include "badges.h"
#include "trainers.h"
bool badgeArtExists(uint8_t i);
extern Pet pet;
void onTap(int16_t x, int16_t y);

static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  setup();
  for (int i=0;i<4;i++) render();
  if (pet.awaitingStarter()) pet.chooseStarter(6);
  if (pet.isEgg()) pet.dbgHatchAs(6,false);
  pet.ageMinutes = 50UL*MINUTES_PER_LEVEL;
  pet.relearnFromLevel();
  while (pet.hasLearnOffer()) pet.declineLearn();

  // Sweep the whole grid area and record which cell each pixel belongs to.
  const int X0 = 40, X1 = 430, Y0 = 258, Y1 = 410;
  int area[4] = {0,0,0,0};
  int overlap = 0;
  int gapRow = 0, gapCol = 0;
  for (int y = Y0; y <= Y1; y++)
    for (int x = X0; x <= X1; x++) {
      int hits = 0, which = -1;
      for (int i = 0; i < 4; i++)
        if (btlCellIndexAt(x, y) == i) { hits++; which = i; }
      // btlCellIndexAt returns the FIRST match, so overlap is found by asking
      // each cell directly instead
      (void)hits;
      if (which >= 0) area[which]++;
    }
  ck(area[0] > 0 && area[1] > 0 && area[2] > 0 && area[3] > 0,
     "all four cells are reachable");
  printf("      areas: %d %d %d %d px\n", area[0], area[1], area[2], area[3]);

  // THE POINT: the bottom row must be at least as easy to hit as the top row.
  ck(area[2] >= area[0] && area[3] >= area[1],
     "the bottom row is not smaller than the top row");

  // no dead horizontal seam between the rows
  int seam = 0;
  for (int y = 316; y <= 328; y++)
    if (btlCellIndexAt(200, y) < 0) seam++;
  ck(seam == 0, "no dead gap between the two rows");
  (void)gapRow; (void)gapCol; (void)overlap;

  // no dead vertical seam between the columns
  int vseam = 0;
  for (int x = 226; x <= 240; x++)
    if (btlCellIndexAt(x, 300) < 0) vseam++;
  ck(vseam == 0, "no dead gap between the two columns");

  // a tap just below the bottom row still counts -- that is where a finger
  // reaching for the lowest button actually lands
  ck(btlCellIndexAt(150, 372) == 2 && btlCellIndexAt(320, 372) == 3,
     "a low tap still reaches the bottom row");

  // The four boxes must TILE: the seam between them is one pixel wide, owned by
  // exactly one side. Checked at the boundaries rather than by re-deriving the
  // rectangles here -- a test that copies the geometry drifts from it.
  ck(btlCellIndexAt(232, 300) == 0 && btlCellIndexAt(233, 300) == 1,
     "the column seam belongs to exactly one side");
  int rowEdge = -1;
  for (int y = 270; y < 400; y++)
    if (btlCellIndexAt(200, y) == 0 && btlCellIndexAt(200, y + 1) == 2) {
      rowEdge = y;
      break;
    }
  ck(rowEdge >= 0, "and so does the row seam");

  // finally, drive a real tap low in the bottom-left cell through battleTap
  startBattle(9, 50);
  btlMenu = 1;
  uint8_t before = btlMenu;
  battleTap(150, 390);
  ck(btlMenu != before, "a low tap in the move grid is actually accepted");

  // Three separate "hard to hit" reports came in from the board, all the same
  // mistake: a button sized to fit its label rather than a finger. This holds
  // every primary control to one minimum so the fourth report does not happen.
  {
    int h[16], n = 0;
    uiButtonHeights(h, 16, &n);
    int small = 0;
    for (int i = 0; i < n; i++) if (h[i] < 44) small++;
    printf("      button heights:");
    for (int i = 0; i < n; i++) printf(" %d", h[i]);
    printf(" px\n");
    ck(small == 0, "every primary button is at least 44 px tall");
  }

  {
    int pt, pb, rt;
    gymHeaderRects(&pt, &pb, &rt);
    ck(pb < rt, "the gym difficulty pill does not sit on the first leader row");
    printf("      pill %d..%d, first row at %d\n", pt, pb, rt);
  }

  // While the pet sleeps only ONE home icon works, and it must be the LIGHT.
  // Removing the ball icon shifted every index by one and left drawButtons()
  // lighting index 2, which had become the BATH -- so the wash button looked
  // like the wake-up button.
  {
    battleOpen = false;        // the battle above owns every tap until it closes
    const int BTN_LIGHT_IDX = uiSleepButton(nullptr, nullptr);
    int lx = 0, ly = 0;
    uiSleepButton(&lx, &ly);
    if (pet.awaitingStarter()) pet.chooseStarter(6);
    if (pet.isEgg()) pet.dbgHatchAs(6, false);
    while (pet.hasLearnOffer()) pet.declineLearn();
    if (!pet.sleeping) pet.toggleLight();
    ck(pet.sleeping, "the pet is asleep");
    // the bath icon must NOT wake it
    int bx = 0, by = 0;
    uiButtonAt(2, &bx, &by, nullptr);
    onTap((int16_t)bx, (int16_t)by);
    ck(pet.sleeping, "the bath icon does not wake a sleeping pet");
    // and what is drawn greyed is exactly what is refused: one answer, so the
    // dimming can never point at a different icon than the tap handler does
    bool grey[4];
    for (int i = 0; i < 4; i++) grey[i] = uiButtonDisabled(i);
    ck(!grey[BTN_LIGHT_IDX] && grey[0] && grey[2] && grey[3],
       "and it is the only icon drawn lit while asleep");

    // the light icon must
    onTap((int16_t)lx, (int16_t)ly);
    ck(!pet.sleeping, "the light icon does");
    for (int i = 0; i < 4; i++)
      if (uiButtonDisabled(i)) bad++, printf("FAIL  icon %d still greyed awake\n", i);
    ck(true, "and awake, every icon is live again");
  }

  // the home icons must not overlap each other now that they are bigger
  {
    int worst = 9999;
    for (int i = 0; i + 1 < 4; i++) {
      int ax, ay, ah, bx2, by2, bh;
      uiButtonAt(i, &ax, &ay, &ah);
      uiButtonAt(i + 1, &bx2, &by2, &bh);
      int gap = (bx2 - bh) - (ax + ah);
      if (gap < worst) worst = gap;
    }
    printf("      smallest gap between home icons: %d px\n", worst);
    ck(worst >= 0, "the home icons do not overlap");
  }

  // EVERY primary control is measured against the FINGER floor, and the ones
  // that cannot reach it are counted rather than hidden.
  //
  // UI_TAP_MIN is 44, which is 44 POINTS misread as pixels: this panel is
  // 466 px over 1.75 in, so a pixel is 0.095 mm, 44 px is 4.2 mm, and the home
  // icons were 5.7 mm. That one measurement is behind all three "hard to hit"
  // reports. UI_TAP_FINGER is 9 mm in this panel's pixels -- asked of the
  // firmware through uiTapFinger(), because a test that retypes the threshold
  // it is enforcing proves nothing.
  //
  // TWO controls are still under it and cannot grow where they are: the LAN
  // button has no room below a full screen, and the battle grid cell would push
  // the grid off the panel. (The party screen's CLOSE bar was a third; it went
  // with the party.) The home icons DO reach
  // it -- they draw at 72 px inside a 94 px hit disc, which is the usual answer
  // here. The count is asserted so a THIRD cannot appear quietly.
  {
    const int FINGER = uiTapFinger();
    const double PPMM = 466.0 / 1.75 / 25.4;
    int h[16], n = 0;
    uiButtonHeights(h, 16, &n);
    int under = 0, tiny = 0;
    printf("      primary controls:");
    for (int i = 0; i < n; i++) printf(" %d(%.1fmm)", h[i], h[i] / PPMM);
    printf("\n");
    for (int i = 0; i < n; i++) {
      if (h[i] < FINGER) under++;
      if (h[i] < 44) tiny++;
    }
    ck(tiny == 0, "no control is below the hard floor");
    // Full-width list rows, judged separately: see uiRowHeights() in the sketch.
    // A 276 x 44 row is a far bigger target than a 44 px button, and five of them
    // at UI_TAP_FINGER would need more panel than the display has -- so the rule
    // for them is the hard floor, enforced rather than assumed.
    {
      int r[8], rn = 0;
      uiRowHeights(r, 8, &rn);
      int thin = 0;
      printf("      list rows:");
      for (int i = 0; i < rn; i++) printf(" %d", r[i]);
      printf(" px\n");
      for (int i = 0; i < rn; i++) if (r[i] < 44) thin++;
      ck(thin == 0, "and no full-width list row is under the hard floor either");
    }
    ck(under <= 2, "and no THIRD control has dropped below the finger floor");

    // The home icons are round and their hit area is a DISC that is bigger than
    // the graphic, so the honest question is whether THAT clears the bezel.
    int offGlass = 0;
    for (int i = 0; i < 4; i++) {
      int bx, by, bh;
      uiButtonAt(i, &bx, &by, &bh);
      double d = sqrt((double)(bx - 233) * (bx - 233) + (double)(by - 233) * (by - 233));
      if (d + uiButtonHitR() > 233) offGlass++;
    }
    ck(offGlass == 0, "every home icon's hit disc is fully on the glass");
  }

  // The two upper care arcs once ran behind the Digimon's name. Ask both
  // renderers for their real bounds: copying either y here would only prove a
  // transcription. Twelve pixels leaves a visible gap at the bitmap font's
  // actual size rather than merely avoiding a shared pixel.
  {
    int clear = 0, bottom = uiHeaderNameBottom();
    for (int i = 0; i < 4; i++) if (uiCareArcTop(i) >= bottom + 12) clear++;
    ck(clear == 4, "every care arc clears the Digimon name band");
  }

  // Three taps hatch an egg, and nothing else on its screen takes a tap.
  {
    pet.newEgg();
    while (!pet.isEgg()) pet.newEgg();
    for (int i = 0; i < 4; i++) onTap(233, 200);
    ck(!pet.isEgg(), "tapping the egg hatches it");
  }

  // The runaway fires on the tap, with NO dialog, and that is deliberate: a
  // creature you have to authorise to leave is not at stake, and neglect
  // having teeth is the premise. What must never happen is REACHING this state
  // by going to sleep -- night_test covers that end of it.
  {
    battleOpen = false;
    if (pet.awaitingStarter()) pet.chooseStarter(6);
    if (pet.isEgg()) pet.dbgHatchAs(25, false);
    while (pet.hasLearnOffer()) pet.declineLearn();
    if (pet.sleeping) pet.toggleLight();
    pet.dbgRunawayReady();
    ck(pet.canRunawayNow(), "total neglect really does make it ready to leave");
    onTap(233, 200);
    ck(pet.ceremony != CER_NONE, "and the tap lets it go, without asking");
  }

  // ---- the confirm panel, shared by every confirm dialog
  {
    int b1t, b1b, b2t, b2b;
    uiConfirmRects(&b1t, &b1b, &b2t, &b2b);
    ck((b1b - b1t) >= 44 && (b2b - b2t) >= 44,
       "both confirm buttons are at least UI_TAP_MIN tall");
    ck(b2t > b1b, "and they do not overlap");
  }

  // ---- a badge per area boss, and none past them
  {
    bool all = true;
    for (uint8_t i = 0; i < TRAINER_GYMS; i++) if (!badgeArtExists(i)) all = false;
    ck(all, "every area boss has badge art");
    ck(!badgeArtExists(TRAINER_GYMS), "and an elite has none");
    ck(!badgeArtExists(250), "nor does a wildly out-of-range index");
  }

  // ---- the memory game's pads, and the training rows that launch it
  //
  // Four big circles on a ring. Two things can go wrong here and neither shows up
  // in a screenshot: pads that OVERLAP, so one tap is inside two of them and the
  // draw path and the tap path can disagree about which; and pads that reach off
  // the round glass, which is the mistake the boss ladder made before
  // uiSafeHalfWidthFor() existed.
  {
    const int r = uiMemoPadR();
    int worstGap = 9999, offGlass = 0, tooSmall = 0;
    for (int i = 0; i < uiMemoPads(); i++) {
      int ax, ay;
      memoPadPos(i, &ax, &ay);
      const int reach = (int)sqrt((double)((ax - 233) * (ax - 233) +
                                           (ay - 233) * (ay - 233))) + r;
      if (reach > 233) offGlass++;
      if (r * 2 < uiTapFinger()) tooSmall++;
      for (int j = i + 1; j < uiMemoPads(); j++) {
        int bx, by;
        memoPadPos(j, &bx, &by);
        const int d = (int)sqrt((double)((ax - bx) * (ax - bx) + (ay - by) * (ay - by)));
        if (d - 2 * r < worstGap) worstGap = d - 2 * r;
      }
    }
    printf("      memo pads: r=%d, closest gap %d px\n", r, worstGap);
    ck(offGlass == 0, "every memory pad is fully on the glass");
    ck(worstGap >= 0, "and no two of them overlap, so a tap is unambiguous");
    ck(tooSmall == 0, "each is at least a finger across");
    int wrong = 0;
    for (int i = 0; i < uiMemoPads(); i++) {
      int px, py;
      memoPadPos(i, &px, &py);
      if (memoPadAt((int16_t)px, (int16_t)py) != i) wrong++;
    }
    ck(wrong == 0, "and tapping a pad's centre resolves to that pad");
    ck(memoPadAt(233, 233) < 0, "the middle belongs to the counter, not to a pad");
  }
  {
    // The training menu grew a fourth row, so its pitch was retuned. The rows must
    // not overlap each other and must stay inside the panel that draws them.
    int overlap = 0, outside = 0;
    for (int i = 0; i < uiTrainRows(); i++) {
      const int top = uiTrainRowTop(i), bot = top + uiTrainRowH();
      if (i && top < uiTrainRowTop(i - 1) + uiTrainRowH()) overlap++;
      if (top < uiTrainPanelTop() || bot > uiTrainPanelBot()) outside++;
    }
    printf("      training rows: %d, panel %d..%d, last ends %d\n",
           uiTrainRows(), uiTrainPanelTop(), uiTrainPanelBot(),
           uiTrainRowTop(uiTrainRows() - 1) + uiTrainRowH());
    ck(overlap == 0, "no two training rows overlap");
    ck(outside == 0, "and every one of them is inside its own panel");
    // Every corner of the panel must be on the glass -- the first attempt at
    // fitting a fourth row made it 332 tall and hung the bottom pair off the bezel.
    int off = 0;
    for (int i = 0; i < 2; i++) {
      const int y = i ? uiTrainPanelBot() : uiTrainPanelTop();
      if (uiSafeHalfWidth(y) < uiTrainPanelHalfW()) off++;
    }
    ck(off == 0, "and the panel's own corners are on the glass");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

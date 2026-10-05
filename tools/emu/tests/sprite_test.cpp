// Which sprite file actually gets opened.
//
// PmdMon::load() once took a uint8_t, so every creature from 256 up wrapped
// onto another one's file. The roster is small now, but the trap is the kind
// that comes back with the next expansion, so this checks EVERY Digimon can be
// asked for and comes back as itself -- by its PIXELS, since every V-Pet
// sprite shares one LCD palette and a palette compare would prove nothing.
#include "Arduino.h"
#include "Arduino_GFX_Library.h"
#include "Preferences.h"
#include "pet.h"
#include "party.h"
#include "sdmon.h"
#include "noart.h"
#include <cstdio>
#include <cstring>
uint32_t g_seed=7; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false;
uint32_t millis(){return 0;}
void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
// sfxPlay comes from host_impl.cpp here -- this test links the host, not the sketch
void emuSetSpriteDir(const char *);
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  emuSetSpriteDir(SPRITE_DIR);

  // two different Digimon really are different pictures, and the colour
  // variant really is a different colouring of the same picture
  {
    PmdMon a, b, v;
    bool okA = a.load(59, false), okB = b.load(3, false), okV = v.load(59, true);
    ck(okA && a.dex == 59, "the last Mega loads and is itself");
    if (okA && okB) {
      const PmdAct &x = a.acts[PMD_IDLE], &y = b.acts[PMD_IDLE];
      bool same = x.w == y.w && x.h == y.h && !memcmp(x.data, y.data, x.w * x.h);
      ck(!same, "H.KABUTERIMON's pixels are not AGUMON's");
    }
    if (okA && okV) {
      const PmdAct &x = a.acts[PMD_IDLE], &y = v.acts[PMD_IDLE];
      ck(x.w == y.w && x.h == y.h && !memcmp(x.data, y.data, x.w * x.h),
         "the colour variant is the same picture");
      ck(memcmp(a.pal, v.pal, a.palCount * sizeof(uint16_t)) != 0,
         "in different colours");
    }
    a.unload(); b.unload(); v.unload();
  }

  // EVERY Digimon: asked for by number, comes back as itself, and no two
  // neighbours draw the same pixels
  {
    int loaded = 0, wrong = 0, missing = 0, twins = 0;
    for (int16_t d = 1; d <= DEX_COUNT; d++) {
      PmdMon m;
      if (!m.load(d, false)) { missing++; continue; }
      loaded++;
      if (m.dex != d) { wrong++; if (wrong < 4) printf("      asked %d, got %d\n", d, m.dex); }
      PmdMon n;
      if (d < DEX_COUNT && n.load(d + 1, false)) {
        const PmdAct &x = m.acts[PMD_IDLE], &y = n.acts[PMD_IDLE];
        if (x.w == y.w && x.h == y.h && !memcmp(x.data, y.data, x.w * x.h)) twins++;
        n.unload();
      }
      m.unload();
    }
    printf("      %d sprites loaded, %d missing from the pack\n", loaded, missing);
    ck(loaded >= DEX_COUNT - NO_ART_COUNT, "every Digimon with art has a sprite to check");
    ck(wrong == 0, "every Digimon that loads is the one that was asked for");
    ck(twins == 0, "and no two neighbours draw the same pixels");
  }

  // and nonsense is refused rather than opening some other creature's file
  {
    PmdMon m;
    ck(!m.load(0, false), "dex 0 loads nothing");
    ck(!m.load(-1, false), "nor a negative dex");
    ck(!m.load(9999, false), "nor one past the table");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

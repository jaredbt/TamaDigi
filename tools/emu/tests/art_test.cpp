// What a form with no art may do: nothing. Only 73 of the 469 forms have public art,
// so this gate is what the rest of the game stands on. A form with no sprite never
// hatches, evolves or appears; and with the sprite pack missing from the card the
// firmware offers nothing rather than a bare dex number. On the other hand a board with
// no SD at all, which never narrows gArtPack, keeps playing.
#include "Arduino.h"
#include "Preferences.h"
#include "pet.h"
#include "wild.h"
#include "noart.h"
#include <cstdio>
#ifndef TAMA_NATIVE_TEST
uint32_t g_seed=8; FakeSerial Serial; FakeESP ESP; FakeWire Wire;
volatile int g_touchX=0,g_touchY=0; volatile bool g_touchDown=false; bool wasPressed=false;
uint32_t millis(){return 0;} void FakeESP::restart(){exit(0);}
int FakeSerial::available(){return 0;} String FakeSerial::readStringUntil(char){return String("");}
void sfxPlay(uint8_t){}
#endif
static int bad=0;
static void ck(bool ok,const char*w){printf("%s  %s\n",ok?"PASS":"FAIL",w); if(!ok)bad++;}

int main(){
  gArtPack = true;

  // --- the SD pack check never demands a file that cannot exist
  {
    bool valid = true;
    int16_t last = 0;
    for (uint8_t slot = 0; slot < 3; slot++) {
      const int16_t probe = artProbe(slot);
      valid = valid && speciesHasArt(probe) && probe > last;
      last = probe;
    }
    ck(valid, "the three pack probes are forms with art, spread across the roster");
    ck(artProbe(3) == 0, "and there is no fourth");
  }

  // --- speciesShowable: art, and the pack
  {
    ck(speciesShowable(6), "BOTAMON can be shown");
    ck(!speciesShowable(1), "KURAMON, with no art, cannot");
    ck(!speciesShowable(0) && !speciesShowable(-1) && !speciesShowable(DEX_COUNT + 1),
       "and neither can a number outside the table");
    gArtPack = false;
    ck(!speciesShowable(6), "with the pack missing, nothing can");
    gArtPack = true;
  }

  // --- the hatchlings are the In-Training I forms with art
  {
    int16_t forms[8];
    const uint8_t n = starterForms(forms, 8);
    ck(n == 3 && forms[0] == 5 && forms[1] == 6 && forms[2] == 7, "PUNIMON, BOTAMON and POYOMON hatch");
    int16_t one[1];
    ck(starterForms(one, 1) == 3 && one[0] == 5, "a short buffer is not overrun, and still counts");
    gArtPack = false;
    ck(starterForms(forms, 8) == 0, "with the pack missing none is offered");
    gArtPack = true;
  }

  // --- no egg ever holds a form that cannot be drawn
  {
    Pet p; p.begin();
    int artless = 0, notHatchling = 0;
    for (int i = 0; i < 300; i++) {
      const int16_t d = p.pickEggSpecies();
      if (!speciesHasArt(d)) artless++;
      if (TS_DEX_TBL[d].stage != TS_ST_IN_TRAINING_1) notHatchling++;
    }
    ck(artless == 0, "no egg ever contains a form with no art");
    ck(notHatchling == 0, "and every egg is an In-Training I");
    // a line with a form not raised yet is preferred, once the first is done
    for (int16_t d = 1; d <= DEX_COUNT; d++) if (speciesHasArt(d) && d != 7) p.dbgRegister(d);
    int seven = 0;
    for (int i = 0; i < 200; i++) if (p.pickEggSpecies() == 7) seven++;
    ck(seven == 200, "an egg leans toward the line with something left to raise");
    p.dbgRegister(7);
    bool varied = false;
    for (int i = 0; i < 200; i++) if (p.pickEggSpecies() != 7) varied = true;
    ck(varied, "and with the Digidex complete any hatchling will do");
    gArtPack = false;
    const int16_t d = p.pickEggSpecies();
    ck(speciesHasArt(d), "with the pack missing, an egg is still a form that has art");
    gArtPack = true;
  }

  // --- no wild Digimon that cannot be drawn
  {
    int artless = 0, wrong = 0, found = 0;
    for (uint8_t st = 0; st < STAGE_COUNT; st++)
      for (uint32_t r = 0; r < 200; r++) {
        const int16_t d = wildPickSpecies(st, r * 2654435761u);
        if (!d) continue;
        found++;
        if (!speciesHasArt(d)) artless++;
        if (TS_DEX_TBL[d].tier != st) wrong++;
      }
    ck(found > 0, "the wild has Digimon in it");
    ck(artless == 0, "none of them without art");
    ck(wrong == 0, "and each from the stage asked for");
    ck(wildPickSpecies(TS_ST_MEGA_PLUS, 0) == 0, "no Mega+ form has art, so that stage is empty");
  }

  // --- 16-bit dex numbers and the bitmap reach the end of the roster
  {
    Pet p; p.begin();
    ck(sizeof(p.dexReg) == (DEX_COUNT + 7) / 8, "the Digidex bitmap covers the whole roster");
    p.dbgHatchAs(444, false);
    p.saveNow();
    Pet q; q.begin();
    ck(q.speciesId == 444, "a dex number past 255 survives a save");
    ck(q.isRegistered(444), "and is registered");
  }

  printf("%s\n", bad?"FAILURES":"all good");
  return bad?1:0;
}

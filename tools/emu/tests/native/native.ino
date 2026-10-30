#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include "pet.h"
#include "inventory.h"
#include "save.h"

Pet pet;
Preferences control;

void sfxPlay(uint8_t) {}

void require(bool ok, const char *message) {
  if (ok) return;
  Serial0.printf("FAIL native: %s\n", message);
  Serial0.flush();
  while (true) delay(1000);
}

void checkMemory() {
  const size_t size = 466 * 466 * 2 + 2 * 135 * 1024;
  require(ESP.getPsramSize() == 8 * 1024 * 1024, "8 MB OPI PSRAM not initialized");
  auto *memory = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM));
  require(memory != nullptr, "framebuffer and battle sprite allocation failed");
  for (size_t index = 0; index < size; index++) memory[index] = uint8_t(index * 17 + 3);
  for (size_t index = 0; index < size; index++)
    require(memory[index] == uint8_t(index * 17 + 3), "PSRAM contents changed");
  heap_caps_free(memory);
  Serial0.println("MILESTONE native-psram-allocation-verified");
}

// A save from before the Time Stranger cutover, on real NVS: the legacy keys, a 151-entry
// Digidex bitmap, and a party and box beside them. saveReplaceOld() has to recognise it
// and clear all of it, keeping only the settings, before anything reads the namespace.
constexpr size_t LEGACY_DEX_BYTES = 19;

void seedLegacy() {
  Preferences seed;
  require(seed.begin("tamadigi", false), "legacy namespace unavailable");
  require(seed.clear(), "legacy seed clear failed");
  require(seed.putBool("init", true) > 0, "legacy init failed");
  require(seed.putChar("spec", 0) > 0, "legacy species failed");
  require(seed.putUInt("age", 30 * MINUTES_PER_LEVEL) > 0, "legacy age failed");
  require(seed.putUChar("ivat", 31) > 0, "legacy IV failed");
  require(seed.putUChar("ivdf", 20) > 0, "legacy IV failed");
  require(seed.putUChar("ivsp", 25) > 0, "legacy IV failed");
  require(seed.putUChar("ivhp", 28) > 0, "legacy IV failed");
  require(seed.putString("nick", "NATIVE") > 0, "legacy nickname failed");
  require(seed.putUShort("badg", 5) > 0, "legacy badges failed");
  uint8_t oldDex[LEGACY_DEX_BYTES];
  memset(oldDex, 0xFF, sizeof(oldDex));
  require(seed.putBytes("dexreg", oldDex, sizeof(oldDex)) == sizeof(oldDex), "legacy dex failed");
  require(seed.putUChar("vol", 3) > 0, "legacy setting failed");
  // A legacy party (six 30-byte records) and an 18-slot box (48-byte records), as old
  // builds stored them, with deliberately dex-number-shaped bytes.
  uint8_t oldParty[6 * 30];
  memset(oldParty, 25, sizeof(oldParty));
  require(seed.putBytes("party", oldParty, sizeof(oldParty)) == sizeof(oldParty), "legacy party failed");
  uint8_t oldBox[18 * 48];
  memset(oldBox, 50, sizeof(oldBox));
  require(seed.putBytes("box", oldBox, sizeof(oldBox)) == sizeof(oldBox), "legacy box failed");
  seed.end();
}

// What the probe raises after the old save is gone, and expects to find on every boot.
constexpr int16_t PARTNER_DEX = 400;   // past 255, on purpose
constexpr uint16_t PARTNER_SKILL = MOVE_COUNT - 1;

void checkCollection() {
  require(pet.speciesId == PARTNER_DEX && pet.ageMinutes == 30 * MINUTES_PER_LEVEL,
          "live creature changed");
  require(pet.ivAtk == 31 && pet.ivDef == 20 && pet.ivSpe == 25 && pet.ivHp == 28,
          "live IVs changed");
  require(pet.moves[0] == PARTNER_SKILL, "16-bit skill id changed");
  require(strcmp(pet.nick, "NATIVE") == 0 && pet.badges == 5, "player or nickname changed");
  require(pet.isRegistered(PARTNER_DEX), "Digidex bit lost");
  require(pet.saveHealthy() && !pet.savePending(), "native saving unhealthy");
}

void setup() {
  Serial0.begin(115200);
  Serial0.println("MILESTONE native-setup-entered");
  require(control.begin("tpnative", false), "probe control namespace unavailable");
  checkMemory();
  const uint8_t phase = control.getUChar("phase", 0);
  if (phase == 0) seedLegacy();
  const bool replaced = saveReplaceOld();
  require(replaced == (phase == 0), "only the seeded old save is replaced");
  pet.begin();
  bag.begin();
  if (phase == 0) {
    // Nothing of the old save may have been read into the new game.
    require(pet.isEgg() && !pet.nick[0] && !pet.badges, "the old save was misread");
    require(!pet.isRegistered(1) && !pet.isRegistered(100), "the old Digidex was misread");
    Preferences look;
    require(look.begin("tamadigi", true), "namespace unavailable");
    require(!look.isKey("party") && !look.isKey("box") && !look.isKey("dexn"), "the old save was left in NVS");
    require(look.getUChar("vol", 0) == 3, "a setting did not survive the replacement");
    look.end();
    Serial0.println("MILESTONE native-real-nvs-legacy-migration");
    pet.dbgHatchAs(PARTNER_DEX, false);
    pet.ageMinutes = 30 * MINUTES_PER_LEVEL;
    pet.formAgeMin = pet.ageMinutes;   // nothing left to learn, so a reload has nothing to save
    pet.ivAtk = 31; pet.ivDef = 20; pet.ivSpe = 25; pet.ivHp = 28;
    pet.moves[0] = PARTNER_SKILL;
    pet.rename("NATIVE");
    pet.badges = 5;
    pet.saveNow();
  }
  checkCollection();
  if (phase == 0) {
    pet.winBadge(9, true);
    require(bag.add(IT_LGRECOVERY, 2) == 2, "inventory reward failed");
    pet.saveNow();
    require(control.putUChar("phase", 1) > 0, "reboot marker failed");
    Serial0.println("MILESTONE native-checkpoints-written-rebooting");
    Serial0.flush();
    ESP.restart();
  }
  require(pet.badgeMask(true) == (1 << 9) && bag.count(IT_LGRECOVERY) == 2,
          "native rewards lost across reboot");
    static uint8_t backup[SAVE_TRANSFER_MAX];
  require(saveExport(backup, sizeof(backup)) > 0, "native checkpoint backup unavailable");
  Serial0.println("MILESTONE native-reboot-reloaded-checkpoints-and-rewards");
  if (phase == 1) {
    require(control.putUChar("phase", 2) > 0, "cold boot marker failed");
    Serial0.println("PASS native warm-reboot complete");
    Serial0.flush();
    return;
  }
  require(phase == 2 || phase == 3, "unexpected probe phase");
  Serial0.println("MILESTONE native-cold-process-flash-persistence");
  if (phase == 3) {
    Serial0.println("MILESTONE native-post-panic-process-persistence");
    Serial0.println("PASS native storage-and-watchdog complete");
    Serial0.flush();
    return;
  }
  const esp_task_wdt_config_t config = { .timeout_ms = 1000, .idle_core_mask = 0, .trigger_panic = true };
  require(esp_task_wdt_reconfigure(&config) == ESP_OK, "task watchdog configuration failed");
  require(esp_task_wdt_add(nullptr) == ESP_OK, "loop task watchdog registration failed");
  require(control.putUChar("phase", 3) > 0, "post-panic boot marker failed");
  Serial0.println("MILESTONE native-watchdog-starvation-armed");
  Serial0.flush();
  while (true) {}
}

void loop() { delay(1000); }
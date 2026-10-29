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

// The "dexreg" key a pre-checkpoint build wrote was a 151-entry bitmap, 19
// bytes. Today's dex may be smaller (87 Digimon: the blob is longer than
// dexReg and only its prefix fits) or larger (the blob is the short one), so
// the bit that must survive is the highest dex BOTH can name -- derived, never
// a literal from whichever roster was current when this was written.
constexpr size_t LEGACY_DEX_BYTES = 19;
constexpr int16_t LEGACY_TOP_DEX =
    DEX_COUNT < int(LEGACY_DEX_BYTES * 8) ? DEX_COUNT : int16_t(LEGACY_DEX_BYTES * 8);

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
  uint8_t oldDex[LEGACY_DEX_BYTES] = {0};
  oldDex[(LEGACY_TOP_DEX - 1) >> 3] |= 1 << ((LEGACY_TOP_DEX - 1) & 7);
  oldDex[LEGACY_DEX_BYTES - 1] |= 1;
  require(seed.putBytes("dexreg", oldDex, sizeof(oldDex)) == sizeof(oldDex), "legacy dex failed");
  // A legacy party (six 30-byte records) and an 18-slot box (48-byte records),
  // as old builds stored them. The party and box were removed by design (one
  // partner), so these are keys the loader must step over without touching the
  // partner; their bytes are deliberately dex-number-shaped.
  uint8_t oldParty[6 * 30];
  memset(oldParty, 25, sizeof(oldParty));
  require(seed.putBytes("party", oldParty, sizeof(oldParty)) == sizeof(oldParty), "legacy party failed");
  uint8_t oldBox[18 * 48];
  memset(oldBox, 50, sizeof(oldBox));
  require(seed.putBytes("box", oldBox, sizeof(oldBox)) == sizeof(oldBox), "legacy box failed");
  seed.end();
}

void checkCollection() {
  require(pet.speciesId == 4 && pet.ageMinutes == 30 * MINUTES_PER_LEVEL,
          "legacy live creature changed");
  require(pet.ivAtk == 31 && pet.ivDef == 20 && pet.ivSpe == 25 && pet.ivHp == 28,
          "live IVs changed");
  require(strcmp(pet.nick, "NATIVE") == 0 && pet.badges == 5, "player or nickname changed");
  require(pet.isRegistered(LEGACY_TOP_DEX) && pet.isRegistered(4), "legacy dex bits lost");
  require(pet.saveHealthy() && !pet.savePending(), "native saving unhealthy");
}

void setup() {
  Serial0.begin(115200);
  Serial0.println("MILESTONE native-setup-entered");
  require(control.begin("tpnative", false), "probe control namespace unavailable");
  checkMemory();
  const uint8_t phase = control.getUChar("phase", 0);
  if (phase == 0) seedLegacy();
  pet.begin();
  bag.begin();
  checkCollection();
  if (phase == 0) {
    Serial0.println("MILESTONE native-real-nvs-legacy-migration");
    pet.winBadge(1, 2, true);
    require(bag.add(IT_MASTERSCAN, 2) == 2, "inventory reward failed");
    pet.saveNow();
    Preferences poison;
    require(poison.begin("tamadigi", false), "poison namespace failed");
    require(poison.putShort("dexn", 1) > 0 && poison.putString("nick", "POISON") > 0 &&
            poison.putUShort("badg", 0) > 0, "legacy poisoning failed");
    poison.end();
    require(control.putUChar("phase", 1) > 0, "reboot marker failed");
    Serial0.println("MILESTONE native-checkpoints-written-rebooting");
    Serial0.flush();
    ESP.restart();
  }
  require(pet.badgeMask(1, true) == (1 << 2) && bag.count(IT_MASTERSCAN) == 2,
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
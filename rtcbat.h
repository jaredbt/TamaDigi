#pragma once
#include <Arduino.h>

// PCF85063 RTC: the time persists while the board has power
bool rtcBegin();
uint32_t rtcEpoch();             // unix seconds; 0 if the RTC is not valid
void rtcSetEpoch(uint32_t e);

// AXP2101 PMU: battery state
bool batBegin();
void pmuEnablePanel();           // turns on BLDO1 (OLED VDD 3.3V); call before gfx->begin()
int batPercent();                // 0-100, -1 if no battery is connected
bool batCharging();
bool usbPresent();

// AXP2101 PWR button: a 4s long press = hard power-off (the RTC stays alive);
// the short press is captured by the firmware (screen on/off)
void pwrSetup();
// Reads the PMU's latched interrupts ONCE and remembers them. Call this exactly
// once per loop pass, before the accessors below -- they consume what it saw and
// do no I2C of their own. See the note in rtcbat.cpp for why there can only be
// one poller.
void pwrPoll();
bool pwrShortPressed();  // screen on/off
// The hold is on its way to the AXP2101's own power-off. This is the only
// warning the firmware gets that the rails are about to drop, so it is the one
// chance to flush a pending save.
bool pwrLongPressed();
// The gauge has dropped past its low-battery warning level. Same purpose: flush
// before a brownout rather than after it.
bool batLowWarning();

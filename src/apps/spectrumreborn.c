#include "spectrumreborn.h"
#include "../dcs.h"
#include "../driver/bk4819.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../helper/lootlist.h"
#include "../helper/measurements.h"
#include "../helper/presetlist.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../svc.h"
#include "../svc_render.h"
#include "../svc_scan.h"
#include "../ui/graphics.h"
#include "../ui/spectrum.h"
#include "../ui/statusline.h"
#include "apps.h"

#define DATA_LEN 128

static const uint16_t U16_MAX = 65535;

static uint8_t noiseOpenDiff = 14;

static const uint8_t SPECTRUM_H = 40;

static const uint8_t SPECTRUM_Y = 16;

static Band *currentBand;

static uint32_t currentStepSize;

static bool newScan = true;

static uint16_t rssiO = U16_MAX;
static uint16_t noiseO = 0;

static uint8_t msmDelay = 5;
static Loot msm = {0};

static uint16_t oldPresetIndex = 255;

static uint32_t timeout = 0;
static bool lastListenState = false;

static const uint16_t BK_RST_HARD = 0x0200;
static const uint16_t BK_RST_SOFT = 0xBFF1 & ~BK4819_REG_30_ENABLE_VCO_CALIB;

static const uint16_t RESET_METHODS[] = {BK_RST_HARD, BK_RST_SOFT};
static const char *RESET_METHOD_NAMES[] = {"Hard", "Soft"};

static uint8_t rssiResetMethod = 0;
static uint16_t resetBkVal = BK_RST_HARD;

static bool isSquelchOpen() { return msm.rssi >= rssiO && msm.noise <= noiseO; }
// static bool isSquelchOpen() { return msm.noise <= 50; }

static void updateMeasurements() {
  if (!gIsListening) {
    BK4819_SetFrequency(msm.f);
    BK4819_WriteRegister(BK4819_REG_30, resetBkVal);
    BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
    SYSTEM_DelayMs(msmDelay); // (X_X)
  }
  msm.blacklist = false;
  msm.rssi = BK4819_GetRSSI();
  msm.noise = BK4819_GetNoise();

  if (gIsListening) {
    noiseO -= noiseOpenDiff;
    msm.open = isSquelchOpen();
    noiseO += noiseOpenDiff;
  } else {
    msm.open = isSquelchOpen();
  }

  if (lastListenState != msm.open) {
    lastListenState = msm.open;
    SetTimeout(&timeout, msm.open ? SCAN_TIMEOUTS[gSettings.sqOpenedTimeout]
                                  : SCAN_TIMEOUTS[gSettings.sqClosedTimeout]);
  }

  if (timeout && CheckTimeout(&timeout)) {
    msm.open = false;
  }

  SP_AddPoint(&msm);
  LOOT_Update(&msm);
  RADIO_ToggleRX(msm.open);
}

static void updateStats() {
  const uint16_t noiseFloor = SP_GetNoiseFloor();
  const uint16_t noiseMax = SP_GetNoiseMax();
  rssiO = noiseFloor;
  noiseO = noiseMax - noiseOpenDiff;
}

static void init() {
  newScan = true;
  timeout = 0;
  oldPresetIndex = 0;
  rssiO = U16_MAX;
  noiseO = 0;

  radio->radio = RADIO_BK4819;

  LOOT_Standby();
  RADIO_SetupBandParams();

  SP_Init(PRESETS_GetSteps(gCurrentPreset), LCD_WIDTH);
}

static void startNewScan() {
  currentBand = &gCurrentPreset->band;
  currentStepSize = PRESETS_GetStepSize(gCurrentPreset);

  msm.f = currentBand->bounds.start;

  if (gSettings.activePreset != oldPresetIndex) {
    init();
    oldPresetIndex = gSettings.activePreset;
  } else {
    SP_Begin();
  }
}

void SPECTRUM_init(void) {
  SVC_Toggle(SVC_LISTEN, false, 0);
  RADIO_LoadCurrentVFO();
  init();
}

void SPECTRUM_deinit() {
  BK4819_WriteRegister(BK4819_REG_30, 0xBFF1);
  RADIO_ToggleRX(false);
  SVC_Toggle(SVC_LISTEN, true, gSettings.scanTimeout);
  RADIO_SetupBandParams();
}

bool SPECTRUM_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  // up-down keys
  if (bKeyPressed || (!bKeyPressed && !bKeyHeld)) {
    switch (Key) {
    case KEY_UP:
      PRESETS_SelectPresetRelative(true);
      RADIO_SelectPresetSave(gSettings.activePreset);
      newScan = true;
      return true;
    case KEY_DOWN:
      PRESETS_SelectPresetRelative(false);
      RADIO_SelectPresetSave(gSettings.activePreset);
      newScan = true;
      return true;
    case KEY_1:
      IncDec8(&msmDelay, 0, 20, 1);
      SP_ResetHistory();
      newScan = true;
      return true;
    case KEY_7:
      IncDec8(&msmDelay, 0, 20, -1);
      SP_ResetHistory();
      newScan = true;
      return true;
    case KEY_3:
      IncDec8(&noiseOpenDiff, 2, 40, 1);
      return true;
    case KEY_9:
      IncDec8(&noiseOpenDiff, 2, 40, -1);
      return true;
    default:
      break;
    }
  }

  // long held
  if (bKeyHeld && bKeyPressed && !gRepeatHeld) {
    switch (Key) {
    case KEY_SIDE1:
      if (gLastActiveLoot) {
        RADIO_TuneToSave(gLastActiveLoot->f);
        APPS_run(APP_ANALYZER);
      }
      return true;
    default:
      break;
    }
  }

  // Simple keypress
  if (!bKeyPressed && !bKeyHeld) {
    switch (Key) {
    case KEY_EXIT:
      APPS_exit();
      return true;
    case KEY_SIDE1:
      LOOT_BlacklistLast();
      return true;
    case KEY_SIDE2:
      LOOT_GoodKnownLast();
      return true;
    case KEY_F:
      APPS_run(APP_PRESET_CFG);
      return true;
    case KEY_0:
      APPS_run(APP_PRESETS_LIST);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_5:
      return true;
    case KEY_4:
      rssiResetMethod = !rssiResetMethod;
      resetBkVal = RESET_METHODS[rssiResetMethod];
      SP_ResetHistory();
      newScan = true;
      return true;
    case KEY_PTT:
      if (gLastActiveLoot) {
        RADIO_TuneToSave(gLastActiveLoot->f);
        APPS_run(APP_STILL);
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void SPECTRUM_update(void) {
  if (Now() - gLastRender >= 500) {
    gRedrawScreen = true;
  }
  if (newScan || gSettings.activePreset != oldPresetIndex) {
    newScan = false;
    startNewScan();
  }
  updateMeasurements();
  if (gIsListening) {
    gRedrawScreen = true;
    return;
  }
  if (msm.f >= currentBand->bounds.end) {
    updateStats();
    gRedrawScreen = true;
    newScan = true;
    return;
  }

  msm.f += currentStepSize;
  SP_Next();
  if (gSettings.skipGarbageFrequencies && (msm.f % 1300000 == 0)) {
    msm.f += currentStepSize;
    SP_Next();
  }
}

void SPECTRUM_render(void) {
  UI_ClearScreen();
  STATUSLINE_SetText(currentBand->name);

  SP_Render(gCurrentPreset, 0, SPECTRUM_Y, SPECTRUM_H);

  PrintSmallEx(0, SPECTRUM_Y - 3, POS_L, C_FILL, "%ums", msmDelay);
  PrintSmallEx(0, SPECTRUM_Y - 3 + 6, POS_L, C_FILL, "%s",
               RESET_METHOD_NAMES[rssiResetMethod]);
  PrintSmallEx(DATA_LEN - 2, SPECTRUM_Y - 3, POS_R, C_FILL, "SQ %u",
               noiseOpenDiff);
  PrintSmallEx(DATA_LEN - 2, SPECTRUM_Y - 3 + 8, POS_R, C_FILL, "%s",
               modulationTypeOptions[currentBand->modulation]);

  if (gLastActiveLoot) {
    PrintMediumBoldEx(LCD_XCENTER, 16, POS_C, C_FILL, "%u.%05u",
                      gLastActiveLoot->f / 100000, gLastActiveLoot->f % 100000);
    if (gLastActiveLoot->ct != 0xFF) {
      PrintSmallEx(LCD_XCENTER, 16 + 6, POS_C, C_FILL, "CT %u.%u",
                   CTCSS_Options[gLastActiveLoot->ct] / 10,
                   CTCSS_Options[gLastActiveLoot->ct] % 10);
    }
  }

  uint32_t fs = currentBand->bounds.start;
  uint32_t fe = currentBand->bounds.end;

  PrintSmallEx(0, LCD_HEIGHT - 1, POS_L, C_FILL, "%u.%05u", fs / 100000,
               fs % 100000);
  PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 1, POS_R, C_FILL, "%u.%05u", fe / 100000,
               fe % 100000);
}

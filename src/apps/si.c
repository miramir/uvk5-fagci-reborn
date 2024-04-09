#include "si.h"
#include "../driver/si473x.h"
#include "../scheduler.h"
#include "../svc.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "apps.h"
#include "finput.h"

static uint32_t freq = 10320;
static uint32_t lastUpdate = 0;
static uint32_t lastRdsUpdate = 0;

static void tune(uint32_t f) { SI4732_SetFreq(freq = f); }

void SI_init() {
  SVC_Toggle(SVC_LISTEN, false, 0);
  BK4819_Idle();
  SI4732_Init();
}

static bool hasRDS = false;

void SI_update() {
  if (Now() - lastRdsUpdate >= 100) {
    hasRDS = SI4732_GetRDS();
    lastRdsUpdate = Now();
  }
  if (hasRDS) {
    gRedrawScreen = true;
  }
  if (Now() - lastUpdate >= 500) {
    RSQ_GET();
    lastUpdate = Now();
    gRedrawScreen = true;
  }
}

bool SI_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld) {
  // up-down keys
  if (bKeyPressed || (!bKeyPressed && !bKeyHeld)) {
    switch (key) {
    case KEY_UP:
      freq += 10;
      SI4732_SetFreq(freq);
      return true;
    case KEY_DOWN:
      freq -= 10;
      SI4732_SetFreq(freq);
      return true;
    default:
      break;
    }
  }

  // long held
  if (bKeyHeld && bKeyPressed && !gRepeatHeld) {
    switch (key) {
    default:
      break;
    }
  }

  // Simple keypress
  if (!bKeyPressed && !bKeyHeld) {
    switch (key) {
    case KEY_0:
    case KEY_1:
    case KEY_2:
    case KEY_3:
    case KEY_4:
    case KEY_5:
    case KEY_6:
    case KEY_7:
    case KEY_8:
    case KEY_9:
      gFInputCallback = tune;
      APPS_run(APP_FINPUT);
      APPS_key(key, bKeyPressed, bKeyHeld);
      return true;
    case KEY_F:
      return true;
    case KEY_STAR:
      BK4819_Idle();
      return true;
    case KEY_EXIT:
      APPS_exit();
      return true;
    default:
      break;
    }
  }
  return false;
}

void SI_render() {
  UI_ClearScreen();
  const uint8_t BASE = 38;

  uint32_t f = freq * 1000;
  uint16_t fp1 = f / 100000;
  uint16_t fp2 = f / 100 % 1000;

  UI_RSSIBar(SI4732_GetRSSI() << 1, f, 42);
  char genre[17];
  SI4732_GetProgramType(genre);

  if (rdsResponse.resp.RDSSYNC) {
    PrintMediumEx(LCD_WIDTH, 14, POS_R, C_FILL, "RDS");
  }
  DateTime *dt;
  Time *t;
  bool hasDT = SI4732_GetLocalDateTime(dt);
  bool hasT = SI4732_GetLocalTime(t);
  const char wd[8][3] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA", "SU"};
  PrintSmall(0, LCD_HEIGHT - 2, "%16s", rds.radioText);
  PrintSmallEx(LCD_XCENTER, 14, POS_C, C_FILL, "%16s", genre);

  if (hasDT && dt->year > 2000 && dt->year < 3000) {
    PrintSmall(0, LCD_HEIGHT - 8, "%02u.%02u.%04u, %s %02u:%02u", dt->day,
               dt->month, dt->year, wd[dt->wday], dt->hour, dt->minute);
  } else if (hasT) {
    PrintSmall(0, LCD_HEIGHT - 8, "%02u:%02u", t->hour, t->minute);
  }

  PrintSmallEx(LCD_WIDTH, LCD_HEIGHT - 8, POS_R, C_FILL, "%s",
               rds.programTypeName);
  PrintSmall(0, 16 + 12, "CC:%u M:%u", rds.extendedCountryCode, rds.music);
  PrintSmall(0, 16 + 18, "SIG:%u", rds.RDSSignal);

  if (gTxState && gTxState != TX_ON) {
    PrintMediumBoldEx(LCD_XCENTER, BASE, POS_C, C_FILL, "%s",
                      TX_STATE_NAMES[gTxState]);
  } else {
    PrintBiggestDigitsEx(LCD_WIDTH - 22, BASE, POS_R, C_FILL, "%3u.%03u", fp1,
                         fp2);
  }
}
void SI_deinit() {
  SI4732_PowerDown();
  BK4819_RX_TurnOn();
  SVC_Toggle(SVC_LISTEN, true, 10);
}

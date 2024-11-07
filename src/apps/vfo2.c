#include "vfo2.h"
#include "../apps/textinput.h"
#include "../dcs.h"
#include "../helper/lootlist.h"
#include "../helper/channels.h"
#include "../helper/numnav.h"
#include "../helper/presetlist.h"
#include "../helper/measurements.h"
#include "../misc.h"
#include "../scheduler.h"
#include "../settings.h"
#include "../svc.h"
#include "../svc_render.h"
#include "../svc_scan.h"
#include "../ui/components.h"
#include "../ui/graphics.h"
#include "../ui/statusline.h"
#include "finput.h"

static bool SSB_Seek_ON = false;
static bool SSB_Seek_UP = true;

static void setChannel(uint16_t v) { RADIO_TuneToCH(v - 1); }
static void tuneTo(uint32_t f) { RADIO_TuneToSave(GetTuneF(f)); }

static char message[16] = {'\0'};
static void sendDtmf() {
  RADIO_ToggleTX(true);
  if (gTxState == TX_ON) {
    BK4819_EnterDTMF_TX(true);
    BK4819_PlayDTMFString(message, true, 100, 100, 100, 100);
    RADIO_ToggleTX(false);
  }
}

static int32_t scanIndex = 0;
static void channelScanFn(bool forward) {
  IncDecI32(&scanIndex, 0, gScanlistSize, forward ? 1 : -1);
  int32_t chNum = gScanlist[scanIndex];
  radio->channel = chNum;
  RADIO_VfoLoadCH(gSettings.activeVFO);
  RADIO_SetupByCurrentVFO();
}

static void prepareABScan() {
  const uint32_t F1 = gVFO[0].rx.f;
  const uint32_t F2 = gVFO[1].rx.f;
  FRange *b = &defaultPreset.band.bounds;

  if (F1 < F2) {
    b->start = F1;
    b->end = F2;
  } else {
    b->start = F2;
    b->end = F1;
  }
  sprintf(defaultPreset.band.name, "%u-%u", b->start / MHZ, b->end / MHZ);
  gCurrentPreset = &defaultPreset;
  defaultPreset.lastUsedFreq = radio->rx.f;
  gSettings.crossBandScan = false;
  RADIO_TuneToPure(b->start, true);
}

static void initChannelScan() {
  scanIndex = 0;
  LOOT_Clear();
  CHANNELS_LoadScanlist(gSettings.currentScanlist);
  if (gScanlistSize == 0) {
    gSettings.currentScanlist = 15;
    CHANNELS_LoadScanlist(gSettings.currentScanlist);
    SETTINGS_DelayedSave();
  }
  for (uint16_t i = 0; i < gScanlistSize; ++i) {
    CH ch;
    int32_t num = gScanlist[i];
    CHANNELS_Load(num, &ch);
    Loot *loot = LOOT_AddEx(ch.rx.f, true);
    loot->open = false;
    loot->lastTimeOpen = 0;
  }

  gScanFn = channelScanFn;
}

static void startScan() {
  if (radio->channel >= 0) {
    initChannelScan();
  }
  if (radio->channel >= 0 && gScanlistSize == 0) {
    SVC_Toggle(SVC_SCAN, false, 0);
    return;
  }
  SVC_Toggle(SVC_SCAN, true, gSettings.scanTimeout);
}

static void scanlistByKey(KEY_Code_t key) {
  if (key >= KEY_1 && key <= KEY_8) {
    gSettings.currentScanlist = key - 1;
  } else {
    gSettings.currentScanlist = 15;
  }
}

static void selectFirstPresetFromScanlist() {
  uint8_t sl = gSettings.currentScanlist;
  uint8_t scanlistMask = 1 << sl;
  for (uint8_t i = 0; i < PRESETS_COUNT; ++i) {
    if (sl == 15 ||
        (PRESETS_Item(i)->memoryBanks & scanlistMask) == scanlistMask) {
      PRESET_Select(i);
      RADIO_TuneTo(gCurrentPreset->band.bounds.start);
      return;
    }
  }
}

static void render2VFOPart(uint8_t i) {
  const uint8_t BASE = 22;
  const uint8_t bl = BASE + 34 * i;

  Preset *p = gVFOPresets[i];
  VFO *vfo = &gVFO[i];
  const bool isActive = gSettings.activeVFO == i;
  const Loot *loot = &gLoot[i];

  uint32_t f =
      gTxState == TX_ON && isActive ? RADIO_GetTXF() : GetScreenF(vfo->rx.f);

  const uint16_t fp1 = f / MHZ;
  const uint16_t fp2 = f / 100 % 1000;
  const uint8_t fp3 = f % 100;
  const char *mod =
      modulationTypeOptions[vfo->modulation == MOD_PRST ? p->band.modulation
                                                        : vfo->modulation];
  const uint32_t step = StepFrequencyTable[p->band.step];

  if (isActive && gTxState <= TX_ON) {
    FillRect(0, bl - 14, 28, 7, C_FILL);
    if (gTxState == TX_ON) {
      PrintMedium(0, bl, "TX");
    }
  }

  if (gIsListening && ((gSettings.dw != DW_OFF && gDW.activityOnVFO == i) ||
                       (gSettings.dw == DW_OFF && isActive))) {
    PrintMedium(0, bl, "RX");
    UI_RSSIBar(gLoot[i].rssi, RADIO_GetS(), f, 31);
  }

  if (gSettings.dw != DW_OFF && gDW.lastActiveVFO == i) {
    PrintMedium(13, bl, ">>");
  }

  if (gTxState && gTxState != TX_ON && isActive) {
    PrintMediumBoldEx(LCD_XCENTER, bl - 8, POS_C, C_FILL, "%s",
                      TX_STATE_NAMES[gTxState]);
    PrintSmallEx(LCD_XCENTER, bl - 8 + 6, POS_C, C_FILL, "%u", RADIO_GetTXF());
  } else {
    if (vfo->channel >= 0) {
      if (gSettings.chDisplayMode == CH_DISPLAY_MODE_F) {
        PrintBigDigitsEx(LCD_WIDTH - 19, bl, POS_R, C_FILL, "%4u.%03u.%02u", fp1,
                         fp2, fp3);
      } else if (gSettings.chDisplayMode == CH_DISPLAY_MODE_N) {
        PrintMediumBoldEx(LCD_XCENTER, bl - 4, POS_C, C_FILL, gVFONames[i]);
      } else {
        PrintMediumBoldEx(LCD_XCENTER, bl - 8, POS_C, C_FILL, gVFONames[i]);
        PrintMediumEx(LCD_XCENTER, bl, POS_C, C_FILL, "%4u.%03u.%02u", fp1, fp2, fp3);
      }
      PrintSmallEx(14, bl - 9, POS_C, C_INVERT, "MR %03u", vfo->channel + 1);
    } else {
      PrintBigDigitsEx(LCD_WIDTH - 19, bl, POS_R, C_FILL, "%4u.%03u", fp1, fp2);
      PrintMediumBoldEx(LCD_WIDTH, bl, POS_R, C_FILL, "%02u", fp3);
      PrintSmallEx(14, bl - 9, POS_C, C_INVERT, "VFO");
    }
    PrintSmallEx(LCD_WIDTH - 1, bl - 9, POS_R, C_FILL, mod);
    if (vfo->modulation != MOD_PRST) {
      FillRect(LCD_WIDTH - 17, bl - 14, 17, 7, C_INVERT);
    }
  }

  Radio r = vfo->radio == RADIO_UNKNOWN ? p->radio : vfo->radio;

  uint32_t est = loot->lastTimeOpen ? (Now() - loot->lastTimeOpen) / 1000 : 0;
  if (r == RADIO_BK4819) {
    if (loot->ct != 0xFF) {
      PrintSmallEx(0, bl + 6, POS_L, C_FILL, "CT %u.%u",
                   CTCSS_Options[loot->ct] / 10, CTCSS_Options[loot->ct] % 10);
    } else if (loot->cd != 0xFF) {
      PrintSmallEx(0, bl + 6, POS_L, C_FILL, "D%03oN", DCS_Options[loot->cd]);
    }
  }
  PrintSmallEx(LCD_XCENTER, bl + 6, POS_C, C_FILL, "%c %c SQ%u %c %s %s",
               p->allowTx ? TX_POWER_NAMES[p->power][0] : ' ',
               "wNnW"[p->band.bw], p -> band.squelch,
               RADIO_GetTXFEx(vfo, p) != vfo->rx.f
                   ? (p->offsetDir ? TX_OFFSET_NAMES[p->offsetDir][0] : '*')
                   : ' ',
               vfo->tx.codeType ? TX_CODE_TYPES[vfo->tx.codeType] : "",
               shortRadioNames[r]);

  if (loot->lastTimeOpen) {
    PrintSmallEx(LCD_WIDTH, bl + 6, POS_R, C_FILL, "%02u:%02u %us", est / 60,
                 est % 60, loot->duration / 1000);
  } else {
    PrintSmallEx(LCD_WIDTH, bl + 6, POS_R, C_FILL, "%d.%02dk", step / 100,
                 step % 100);
  }
}

void VFO2_init(void) {
  gDW.activityOnVFO = -1;
  RADIO_LoadCurrentVFO();
}

bool VFO2_key(KEY_Code_t key, bool bKeyPressed, bool bKeyHeld) {
  if (!SVC_Running(SVC_SCAN) && !bKeyPressed && !bKeyHeld &&
      radio->channel >= 0) {
    if (!gIsNumNavInput && key <= KEY_9) {
      NUMNAV_Init(radio->channel + 1, 1, CHANNELS_GetCountMax());
      gNumNavCallback = setChannel;
    }
    if (gIsNumNavInput) {
      NUMNAV_Input(key);
      return true;
    }
  }
  if (key == KEY_PTT && !gIsNumNavInput) {
    RADIO_ToggleTX(bKeyHeld);
    return true;
  }

  // up-down keys
  if (bKeyPressed || (!bKeyPressed && !bKeyHeld)) {
    bool isSsb = RADIO_IsSSB();
    switch (key) {
    case KEY_UP:
      if (SSB_Seek_ON) {
        SSB_Seek_UP = true;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        gScanForward = true;
      }
      RADIO_NextFreqNoClicks(true);
      return true;
    case KEY_DOWN:
      if (SSB_Seek_ON) {
        SSB_Seek_UP = false;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        gScanForward = false;
      }
      RADIO_NextFreqNoClicks(false);
      return true;
    case KEY_SIDE1:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f + 5);
        return true;
      }
      break;
    case KEY_SIDE2:
      if (RADIO_GetRadio() == RADIO_SI4732 && isSsb) {
        RADIO_TuneToSave(radio->rx.f - 5);
        return true;
      }
      break;
    default:
      break;
    }
  }

  // long held
  if (bKeyHeld && bKeyPressed && !gRepeatHeld) {
    OffsetDirection offsetDirection = gCurrentPreset->offsetDir;
    switch (key) {
    case KEY_EXIT:
      prepareABScan();
      startScan();
      return true;
    case KEY_1:
      APPS_run(APP_PRESETS_LIST);
      return true;
    case KEY_2:
      LOOT_Standby();
      RADIO_NextVFO();
      return true;
    case KEY_3:
      RADIO_ToggleVfoMR();
      return true;
    case KEY_4: // freq catch
      if (RADIO_GetRadio() == RADIO_BK4819) {
        SVC_Toggle(SVC_FC, !SVC_Running(SVC_FC), 100);
      } else {
        gShowAllRSSI = !gShowAllRSSI;
      }
      return true;
    case KEY_5: // noaa
      SVC_Toggle(SVC_BEACON, !SVC_Running(SVC_BEACON), 15000);
      return true;
    case KEY_6:
      RADIO_ToggleTxPower();
      return true;
    case KEY_7:
      RADIO_UpdateStep(true);
      return true;
    case KEY_8:
      IncDec8(&offsetDirection, 0, OFFSET_MINUS, 1);
      gCurrentPreset->offsetDir = offsetDirection;
      return true;
    case KEY_9: // call
      gTextInputSize = 15;
      gTextinputText = message;
      gTextInputCallback = sendDtmf;
      APPS_run(APP_TEXTINPUT);
      return true;
    case KEY_0:
      RADIO_ToggleModulation();
      return true;
    case KEY_STAR:
      if (RADIO_GetRadio() == RADIO_SI4732 && RADIO_IsSSB()) {
        SSB_Seek_ON = true;
        // todo: scan by snr
      } else {
        if (gSettings.crossBandScan && radio->channel <= -1) {
          selectFirstPresetFromScanlist();
        }
        startScan();
      }
      return true;
    case KEY_SIDE1:
      APPS_run(APP_ANALYZER);
      return true;
    case KEY_SIDE2:
      APPS_run(APP_SPECTRUM);
      return true;
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
      if (SVC_Running(SVC_SCAN)) {
        if (radio->channel <= -1) {
          if (gSettings.crossBandScan) {
            scanlistByKey(key);
            selectFirstPresetFromScanlist();
          } else {
            RADIO_SelectPresetSave(key + 6);
          }
        } else {
          scanlistByKey(key);
          initChannelScan();
          SETTINGS_DelayedSave();
        }
      } else {
        gFInputCallback = tuneTo;
        APPS_run(APP_FINPUT);
        APPS_key(key, bKeyPressed, bKeyHeld);
      }
      return true;
    case KEY_F:
      APPS_run(APP_VFO_CFG);
      return true;
    case KEY_STAR:
      APPS_run(APP_LOOT_LIST);
      return true;
    case KEY_SIDE1:
      if (SVC_Running(SVC_SCAN)) {
        LOOT_BlacklistLast();
        RADIO_NextFreqNoClicks(true);
        return true;
      }
      gMonitorMode = !gMonitorMode;
      return true;
    case KEY_SIDE2:
      if (SVC_Running(SVC_SCAN)) {
        LOOT_WhitelistLast();
        RADIO_NextFreqNoClicks(true);
        return true;
      }
      break;
    case KEY_EXIT:
      if (SSB_Seek_ON) {
        SSB_Seek_ON = false;
        return true;
      }
      if (SVC_Running(SVC_SCAN)) {
        SVC_Toggle(SVC_SCAN, false, 0);
        return true;
      }
      if (!APPS_exit()) {
        SVC_Toggle(SVC_SCAN, false, 0);
        LOOT_Standby();
        RADIO_NextVFO();
      }
      return true;
    default:
      break;
    }
  }
  return false;
}

void VFO2_update(void) {   
  if (SSB_Seek_ON) {
    if (RADIO_GetRadio() == RADIO_SI4732 && RADIO_IsSSB()) {
      if (Now() - gLastRender >= 150) {
        if (SSB_Seek_UP) {
          gScanForward = true;
          RADIO_NextFreqNoClicks(true);
        } else {
          gScanForward = false;
          RADIO_NextFreqNoClicks(false);
        }
        gRedrawScreen = true;
      }
    }
  }

  if (gIsListening && Now() - gLastRender >= 500) {
    gRedrawScreen = true;
  } 
}

void VFO2_render(void) {
  UI_ClearScreen();

  if (gIsNumNavInput) {
    STATUSLINE_SetText("Select: %s", gNumNavInput);
  } else {
    STATUSLINE_SetText("%s:%u", gCurrentPreset->band.name,
                       PRESETS_GetChannel(gCurrentPreset, radio->rx.f) + 1);
  }

  render2VFOPart(0);
  render2VFOPart(1);
}

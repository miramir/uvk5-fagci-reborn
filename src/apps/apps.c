#include "apps.h"
#include "../driver/st7565.h"
#include "../ui/statusline.h"
#include "about.h"
#include "analyzer.h"
#include "appslist.h"
#include "finput.h"
#include "generator.h"
#include "level.h"
#include "lootlist.h"
#include "memview.h"
#include "presetcfg.h"
#include "presetlist.h"
#include "reset.h"
#include "savech.h"
#include "settings.h"
#include "spectrumreborn.h"
#include "textinput.h"
#include "vfo2.h"
#include "vfocfg.h"
#include "vfopro.h"

#define APPS_STACK_SIZE 8

AppType_t gCurrentApp = APP_NONE;

static AppType_t appsStack[APPS_STACK_SIZE] = {APP_NONE};
static int8_t stackIndex = -1;

static bool pushApp(AppType_t app) {
  if (stackIndex < APPS_STACK_SIZE - 1) {
    appsStack[++stackIndex] = app;
  } else {
    for (uint8_t i = 1; i < APPS_STACK_SIZE; ++i) {
      appsStack[i - 1] = appsStack[i];
    }
    appsStack[stackIndex] = app;
  }
  return true;
}

static AppType_t popApp(void) {
  if (stackIndex > 0) {
    return appsStack[stackIndex--]; // Do not care about existing value
  }
  return appsStack[stackIndex];
}

AppType_t APPS_Peek(void) {
  if (stackIndex >= 0) {
    return appsStack[stackIndex];
  }
  return APP_NONE;
}

const AppType_t appsAvailableToRun[RUN_APPS_COUNT] = {
    APP_VFO1,         //
    APP_VFOPRO,       //
    APP_VFO2,         //
    APP_SAVECH,       //
    APP_SPECTRUM,     //
    APP_ANALYZER,     //
    APP_LOOT_LIST,    //
    APP_PRESETS_LIST, //
    APP_MEMVIEW,      //
    APP_GENERATOR,    //
    // APP_LEVEL,        //
    APP_ABOUT,        //
};

const App apps[APPS_COUNT] = {
    {"None", NULL, NULL, NULL, NULL, NULL},
    {"EEPROM view", MEMVIEW_Init, NULL, MEMVIEW_Render, MEMVIEW_key, NULL},
    {"Band scan", SPECTRUM_init, SPECTRUM_update, SPECTRUM_render, SPECTRUM_key,
     SPECTRUM_deinit},
    {"Analyzer", ANALYZER_init, ANALYZER_update, ANALYZER_render, ANALYZER_key,
     ANALYZER_deinit},
    {"Channels", SAVECH_init, SAVECH_update, SAVECH_render, SAVECH_key, NULL},
    {"1 VFO pro", VFOPRO_init, VFOPRO_update, VFOPRO_render, VFOPRO_key,
     VFOPRO_deinit},
    {"Freq input", FINPUT_init, NULL, FINPUT_render, FINPUT_key, FINPUT_deinit},
    {"Run app", APPSLIST_init, NULL, APPSLIST_render, APPSLIST_key, NULL},
    {"Loot", LOOTLIST_init, LOOTLIST_update, LOOTLIST_render, LOOTLIST_key,
     NULL},
    {"Presets", PRESETLIST_init, NULL, PRESETLIST_render, PRESETLIST_key, NULL},
    {"Reset", RESET_Init, RESET_Update, RESET_Render, RESET_key, NULL},
    {"Text input", TEXTINPUT_init, NULL, TEXTINPUT_render, TEXTINPUT_key,
     TEXTINPUT_deinit},
    {"VFO cfg", VFOCFG_init, VFOCFG_update, VFOCFG_render, VFOCFG_key, NULL},
    {"Preset cfg", PRESETCFG_init, PRESETCFG_update, PRESETCFG_render,
     PRESETCFG_key, NULL},
    {"Settings", SETTINGS_init, NULL, SETTINGS_render, SETTINGS_key, NULL},
    {"2 VFO", VFO2_init, VFO2_update, VFO2_render, VFO2_key, NULL},
    {"Generator", GENERATOR_init, GENERATOR_update, GENERATOR_render,
     GENERATOR_key, NULL},
    // {"Level", LEVEL_init, LEVEL_update, LEVEL_render, LEVEL_key, LEVEL_deinit},
    {"ABOUT", NULL, NULL, ABOUT_Render, ABOUT_key, NULL},
};

bool APPS_key(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld) {
  if (apps[gCurrentApp].key) {
    return apps[gCurrentApp].key(Key, bKeyPressed, bKeyHeld);
  }
  return false;
}

void APPS_init(AppType_t app) {
  gCurrentApp = app;

  STATUSLINE_SetText("%s", apps[gCurrentApp].name);
  gRedrawScreen = true;

  if (apps[gCurrentApp].init) {
    apps[gCurrentApp].init();
  }
}

void APPS_update(void) {
  if (apps[gCurrentApp].update) {
    apps[gCurrentApp].update();
  }
}

void APPS_render(void) {
  if (apps[gCurrentApp].render) {
    apps[gCurrentApp].render();
  }
}

void APPS_deinit(void) {
  if (apps[gCurrentApp].deinit) {
    apps[gCurrentApp].deinit();
  }
}

void APPS_run(AppType_t app) {
  if (appsStack[stackIndex] == app) {
    return;
  }
  if (app != APP_FINPUT && app != APP_TEXTINPUT) {
    APPS_deinit();
  }
  pushApp(app);
  APPS_init(app);
}

void APPS_runManual(AppType_t app) {
  APPS_run(app);
  /* APPS_deinit();
  stackIndex = 0;
  appsStack[stackIndex] = app;
  APPS_init(app); */
}

bool APPS_exit(void) {
  if (stackIndex == 0) {
    return false;
  }
  APPS_deinit();
  AppType_t app = popApp();
  if (app != APP_FINPUT && app != APP_TEXTINPUT) {
    APPS_init(APPS_Peek());
  } else {
    gCurrentApp = APPS_Peek();

    STATUSLINE_SetText("%s", apps[gCurrentApp].name);
    gRedrawScreen = true;
  }
  return true;
}

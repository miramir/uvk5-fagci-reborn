#include "radio.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/system.h"
#include "inc/dp32g030/gpio.h"

const uint16_t StepFrequencyTable[12] = {
    1,   10,  50,  100,

    250, 500, 625, 833, 1000, 1250, 2500, 10000,
};

const uint32_t upConverterValues[] = {0, 5000000, 12500000};

const char *modulationTypeOptions[5] = {" FM", " AM", "SSB", "BYP", "RAW"};
const char *vfoStateNames[] = {
    "NORMAL", "BUSY", "BAT LOW", "DISABLE", "TIMEOUT", "ALARM", "VOL HIGH",
};
const char *powerNames[] = {"LOW", "MID", "HIGH"};
const char *bwNames[3] = {"  25k", "12.5k", "6.25k"};
const char *deviationNames[] = {"", "+", "-"};

UpconverterTypes upconverterType = UPCONVERTER_OFF;

void RADIO_SetupRegisters() {
  uint32_t Frequency = 0;

  GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_AUDIO_PATH);
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_GREEN, false);
  BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);
  BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, false);

  BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE);

  BK4819_SetupPowerAmplifier(0, 0);

  while (BK4819_ReadRegister(BK4819_REG_0C) & 1U) {
    BK4819_WriteRegister(BK4819_REG_02, 0);
    SYSTEM_DelayMs(1);
  }
  BK4819_WriteRegister(BK4819_REG_3F, 0);
  BK4819_WriteRegister(BK4819_REG_7D, 0xE94F); // TODO: maybe add some value
  BK4819_SetFrequency(Frequency);
  BK4819_SelectFilter(Frequency);
  BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, true);
  BK4819_WriteRegister(BK4819_REG_48, 0xB3A8);

  BK4819_DisableScramble();

  BK4819_DisableVox();
  BK4819_DisableDTMF();

  BK4819_WriteRegister(BK4819_REG_3F, BK4819_REG_3F_SQUELCH_FOUND |
                                          BK4819_REG_3F_SQUELCH_LOST);
  BK4819_WriteRegister(0x40, (BK4819_ReadRegister(0x40) & ~(0b11111111111)) |
                                 0b10110101010);
}

uint32_t GetScreenF(uint32_t f) {
  return f - upConverterValues[upconverterType];
}
uint32_t GetTuneF(uint32_t f) { return f + upConverterValues[upconverterType]; }
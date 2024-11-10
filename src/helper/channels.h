#ifndef CHANNELS_H
#define CHANNELS_H

#include "helper/common_types.h"
#include "driver/bk4819.h"
#include <stdint.h>
#include <stdbool.h>

#define SCANLIST_MAX 1024
#define CH_SIZE sizeof(CH)

typedef struct {
  F rx;
  F tx;
  char name[10];
  uint8_t memoryBanks;
  ModulationType modulation : 4;
  BK4819_FilterBandwidth_t bw : 2;
  TXOutputPower power : 2;
  Radio radio : 2;
} __attribute__((packed)) CH; // 22 B
// getsize(CH)

uint16_t CHANNELS_GetCountMax();
void CHANNELS_Load(int16_t num, CH *p);
void CHANNELS_Save(int16_t num, CH *p);
CH *CHANNELS_Get(int16_t i);
bool CHANNELS_LoadBuf();
int16_t CHANNELS_Next(int16_t base, bool next);
void CHANNELS_Delete(int16_t i);
bool CHANNELS_Existing(int16_t i);
uint8_t CHANNELS_Scanlists(int16_t i);
void CHANNELS_LoadScanlist(uint8_t n);
F CHANNELS_GetRX(int16_t num);
void CHANNELS_LoadBlacklistToLoot();

extern int16_t gScanlistSize;
extern uint16_t gScanlist[SCANLIST_MAX];

#endif /* end of include guard: CHANNELS_H */

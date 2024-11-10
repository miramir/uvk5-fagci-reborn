#ifndef VFOS_H
#define VFOS_H

#include "helper/common_types.h"
#include "driver/bk4819.h"
#include <stdint.h>

typedef struct {
  F rx;
  F tx;
  int16_t channel;
  ModulationType modulation : 4;
  TXOutputPower power : 2;
  Radio radio : 2;
} __attribute__((packed)) VFO;
// getsize(VFO)

void VFOS_Load(uint16_t num, VFO *p);
void VFOS_Save(uint16_t num, VFO *p);

#endif /* end of include guard: VFOS_H */

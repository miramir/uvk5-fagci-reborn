#include "vfos.h"
#include "../driver/eeprom.h"
#include "settings.h"

void VFOS_Load(uint16_t num, VFO *p) {
  EEPROM_ReadBuffer(VFOS_OFFSET + num * VFO_SIZE, p, VFO_SIZE);
}

void VFOS_Save(uint16_t num, VFO *p) {
  EEPROM_WriteBuffer(VFOS_OFFSET + num * VFO_SIZE, p, VFO_SIZE);
}

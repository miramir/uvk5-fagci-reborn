#ifndef ADAPTER_H
#define ADAPTER_H

#include "channels.h"
#include "vfos.h"
#include "settings.h"

void VFO2CH(VFO *src, Preset *p, CH *dst);
void CH2VFO(CH *src, VFO *dst);

#endif /* end of include guard: ADAPTER_H */

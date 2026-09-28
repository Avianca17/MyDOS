#ifndef SYSTEM_H
#define SYSTEM_H

void cpuid_vendor(char out[13]);
unsigned int get_total_ram_mb(unsigned int mbi);
#include "../fs/fatfs/source/ff.h"

FRESULT get_storage_info(unsigned int *used_mb, unsigned int *total_mb);

#endif

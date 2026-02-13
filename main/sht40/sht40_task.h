#pragma once
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

void sht40_task_start(i2c_master_dev_handle_t dev);

#ifdef __cplusplus
}
#endif

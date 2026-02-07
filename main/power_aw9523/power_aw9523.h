#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"


#ifdef __cplusplus
extern "C" {
#endif

esp_err_t aw9523_enable_grove_5v(i2c_master_bus_handle_t sys_bus);

#ifdef __cplusplus
}
#endif
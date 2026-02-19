#pragma once

#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the SHT40 polling task.
 *
 * The task periodically requests a high-precision measurement from the SHT40
 * over the provided Port A I2C device handle, then logs temperature and
 * relative humidity.
 *
 * @param dev I2C device handle for the SHT40 (already added to the Port A bus).
 */
void sht40_task_start(i2c_master_dev_handle_t dev);

#ifdef __cplusplus
}
#endif

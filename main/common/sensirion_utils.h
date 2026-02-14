#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute Sensirion CRC-8.
 *
 * Polynomial: 0x31
 * Init value: 0xFF
 *
 * Used by SHT40, SGP30, and other Sensirion sensors.
 */
uint8_t sensirion_crc8(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#include "sensirion_utils.h"

uint8_t sensirion_crc8(const uint8_t *data, int len) {
	uint8_t crc = 0xFF;

	for (int i = 0; i < len; i++) {
		crc ^= data[i];

		for (int bit = 0; bit < 8; bit++) {
			if (crc & 0x80)
				crc = (uint8_t)((crc << 1) ^ 0x31);
			else
				crc <<= 1;
		}
	}

	return crc;
}

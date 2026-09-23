#include "controllar/Crc16.h"

namespace controllar {

uint16_t crc16(const uint8_t* data, size_t length, uint16_t seed) {
    uint16_t crc = seed;
    if (data == nullptr) {
        return crc;
    }
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(static_cast<uint16_t>(data[i]) << 8);
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0u) {
                crc = static_cast<uint16_t>(static_cast<uint16_t>(crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

}  // namespace controllar

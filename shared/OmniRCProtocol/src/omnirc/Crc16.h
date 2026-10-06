#pragma once

#include <stddef.h>
#include <stdint.h>

namespace omnirc {

/// CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no input or
/// output reflection and no final XOR.
///
/// This guards against corruption that the radio layer does not catch, such as
/// a truncated payload or a frame decoded with the wrong length. The check
/// value for the ASCII string "123456789" is 0x29B1.
///
/// The implementation is bitwise rather than table driven: frames are at most
/// 31 bytes, and an 8-bit receiver should not have to spend 512 bytes of flash
/// on a lookup table.
uint16_t crc16(const uint8_t* data, size_t length, uint16_t seed = 0xFFFFu);

}  // namespace omnirc

#include "controllar/Protocol.h"

#include "controllar/Crc16.h"

namespace controllar {
namespace {

/// Both frame types share a layout, so encoding and decoding differ only by
/// magic byte and maximum value count.
uint8_t encodeFrame(uint8_t magic, uint8_t byte2, uint8_t byte3, const int16_t* values,
                    uint8_t count, uint8_t maxCount, uint8_t* out, uint8_t capacity) {
    if (out == nullptr || count > maxCount) {
        return 0;
    }
    const uint8_t size = frameSize(count);
    if (capacity < size) {
        return 0;
    }

    out[0] = magic;
    out[1] = kProtocolVersion;
    out[2] = byte2;
    out[3] = byte3;
    out[4] = count;

    uint8_t offset = kHeaderSize;
    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t raw = static_cast<uint16_t>(values[i]);
        out[offset++] = static_cast<uint8_t>(raw & 0xFFu);
        out[offset++] = static_cast<uint8_t>((raw >> 8) & 0xFFu);
    }

    const uint16_t crc = crc16(out, offset);
    out[offset++] = static_cast<uint8_t>(crc & 0xFFu);
    out[offset++] = static_cast<uint8_t>((crc >> 8) & 0xFFu);

    return size;
}

DecodeError decodeFrame(uint8_t magic, const uint8_t* data, uint8_t length, uint8_t maxCount,
                        uint8_t& byte2, uint8_t& byte3, int16_t* values, uint8_t& count) {
    if (data == nullptr || length < kMinFrameSize) {
        return DecodeError::TooShort;
    }
    if (data[0] != magic) {
        return DecodeError::BadMagic;
    }
    if (data[1] != kProtocolVersion) {
        return DecodeError::UnsupportedVersion;
    }

    const uint8_t declared = data[4];
    if (declared > maxCount) {
        return DecodeError::BadCount;
    }
    const uint8_t expected = frameSize(declared);
    if (length != expected) {
        return DecodeError::LengthMismatch;
    }

    const uint8_t payloadEnd = static_cast<uint8_t>(expected - kCrcSize);
    const uint16_t actual = crc16(data, payloadEnd);
    const uint16_t carried = static_cast<uint16_t>(
        data[payloadEnd] | (static_cast<uint16_t>(data[payloadEnd + 1]) << 8));
    if (actual != carried) {
        return DecodeError::BadCrc;
    }

    // Everything is validated, so it is now safe to write to the caller's frame.
    byte2 = data[2];
    byte3 = data[3];
    count = declared;
    uint8_t offset = kHeaderSize;
    for (uint8_t i = 0; i < declared; ++i) {
        const uint16_t raw =
            static_cast<uint16_t>(data[offset] | (static_cast<uint16_t>(data[offset + 1]) << 8));
        values[i] = static_cast<int16_t>(raw);
        offset = static_cast<uint8_t>(offset + 2);
    }
    return DecodeError::Ok;
}

}  // namespace

const char* toString(DecodeError error) {
    switch (error) {
        case DecodeError::Ok:
            return "ok";
        case DecodeError::TooShort:
            return "too short";
        case DecodeError::BadMagic:
            return "bad magic";
        case DecodeError::UnsupportedVersion:
            return "unsupported version";
        case DecodeError::BadCount:
            return "bad count";
        case DecodeError::LengthMismatch:
            return "length mismatch";
        case DecodeError::BadCrc:
            return "bad crc";
    }
    return "unknown";
}

uint8_t encodeControl(const ControlFrame& frame, uint8_t* out, uint8_t capacity) {
    return encodeFrame(kControlMagic, frame.seq, frame.flags, frame.channels, frame.channelCount,
                       kMaxChannels, out, capacity);
}

DecodeError decodeControl(const uint8_t* data, uint8_t length, ControlFrame& out) {
    ControlFrame parsed;
    const DecodeError error = decodeFrame(kControlMagic, data, length, kMaxChannels, parsed.seq,
                                          parsed.flags, parsed.channels, parsed.channelCount);
    if (error != DecodeError::Ok) {
        return error;
    }
    out = parsed;
    return DecodeError::Ok;
}

uint8_t encodeTelemetry(const TelemetryFrame& frame, uint8_t* out, uint8_t capacity) {
    return encodeFrame(kTelemetryMagic, frame.ackSeq, frame.status, frame.values, frame.valueCount,
                       kMaxTelemetryValues, out, capacity);
}

DecodeError decodeTelemetry(const uint8_t* data, uint8_t length, TelemetryFrame& out) {
    TelemetryFrame parsed;
    const DecodeError error =
        decodeFrame(kTelemetryMagic, data, length, kMaxTelemetryValues, parsed.ackSeq,
                    parsed.status, parsed.values, parsed.valueCount);
    if (error != DecodeError::Ok) {
        return error;
    }
    out = parsed;
    return DecodeError::Ok;
}

}  // namespace controllar

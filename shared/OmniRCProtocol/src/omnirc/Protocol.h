#pragma once

#include <stddef.h>
#include <stdint.h>

/// OmniRC wire protocol.
///
/// Both frame types share one layout, which keeps the encoder, the decoder and
/// the receiver side parser small:
///
///     offset  size   field
///     0       1      magic          0xC7 control, 0xC8 telemetry
///     1       1      version        kProtocolVersion
///     2       1      seq / ackSeq
///     3       1      flags / status
///     4       1      count          number of 16-bit values that follow
///     5       2*N    values         little endian int16
///     5+2N    2      crc            CRC-16/CCITT-FALSE over bytes [0, 5+2N)
///
/// Frames are variable length, so a profile that only needs three channels
/// sends 13 bytes instead of 31. Shorter frames spend less time on air, which
/// directly improves range and packet loss.
///
/// Values are serialised byte by byte instead of memcpy-ing a packed struct.
/// That removes struct padding, alignment and endianness from the list of
/// things that can silently differ between an ESP32 transmitter and an AVR
/// receiver.
namespace omnirc {

constexpr uint8_t kProtocolVersion = 1;

constexpr uint8_t kControlMagic = 0xC7u;
constexpr uint8_t kTelemetryMagic = 0xC8u;

constexpr uint8_t kMaxChannels = 12;
constexpr uint8_t kMaxTelemetryValues = 12;

constexpr uint8_t kHeaderSize = 5;
constexpr uint8_t kCrcSize = 2;

/// Payload limit of the most constrained transport (nRF24L01+). Every frame
/// must fit so that a profile works identically on every transport.
constexpr uint8_t kMaxPayloadSize = 32;

/// Encoded size of a frame carrying `count` 16-bit values.
constexpr uint8_t frameSize(uint8_t count) {
    return static_cast<uint8_t>(kHeaderSize + 2u * count + kCrcSize);
}

constexpr uint8_t kMinFrameSize = frameSize(0);
constexpr uint8_t kMaxControlFrameSize = frameSize(kMaxChannels);
constexpr uint8_t kMaxTelemetryFrameSize = frameSize(kMaxTelemetryValues);

static_assert(kMaxControlFrameSize <= kMaxPayloadSize,
              "Control frame must fit in a single nRF24L01+ payload");
static_assert(kMaxTelemetryFrameSize <= kMaxPayloadSize,
              "Telemetry frame must fit in a single nRF24L01+ ACK payload");

/// Bits in ControlFrame::flags.
namespace ControlFlag {
constexpr uint8_t kArmed = 1u << 0;            ///< Operator has armed the vehicle.
constexpr uint8_t kFailsafeRequest = 1u << 1;  ///< Enter failsafe immediately.
}  // namespace ControlFlag

/// Bits in TelemetryFrame::status.
namespace TelemetryStatus {
constexpr uint8_t kArmed = 1u << 0;           ///< Receiver considers itself armed.
constexpr uint8_t kFailsafeActive = 1u << 1;  ///< Receiver is running its failsafe action.
constexpr uint8_t kLowBattery = 1u << 2;      ///< Vehicle battery below its warning level.
constexpr uint8_t kError = 1u << 3;           ///< Receiver reports a fault.
}  // namespace TelemetryStatus

/// Transmitter to receiver.
struct ControlFrame {
    uint8_t seq = 0;           ///< Rolling counter, used for loss and latency statistics.
    uint8_t flags = 0;         ///< See ControlFlag.
    uint8_t channelCount = 0;  ///< Number of valid entries in `channels`.
    int16_t channels[kMaxChannels] = {};
};

/// Receiver to transmitter. Carried in the nRF24 ACK payload, so it costs no
/// extra air time, and sent as a normal packet on bidirectional transports.
struct TelemetryFrame {
    uint8_t ackSeq = 0;      ///< Sequence number of the last control frame seen.
    uint8_t status = 0;      ///< See TelemetryStatus.
    uint8_t valueCount = 0;  ///< Number of valid entries in `values`.
    int16_t values[kMaxTelemetryValues] = {};
};

/// Why a buffer could not be decoded. Callers should treat anything other than
/// `Ok` as "drop this frame" and never act on partially decoded data.
enum class DecodeError : uint8_t {
    Ok = 0,
    TooShort,            ///< Fewer bytes than the smallest valid frame.
    BadMagic,            ///< Not a frame of the requested type.
    UnsupportedVersion,  ///< Transmitter and receiver run different protocol versions.
    BadCount,            ///< Declared value count exceeds the protocol maximum.
    LengthMismatch,      ///< Buffer length does not match the declared count.
    BadCrc,              ///< Payload was corrupted in transit.
};

/// Human readable name of a decode error, for logs and diagnostic screens.
/// Never returns nullptr.
const char* toString(DecodeError error);

/// Serialises `frame` into `out`.
/// Returns the number of bytes written, or 0 if the frame is invalid or the
/// buffer is too small. A return value of 0 always means nothing was written.
uint8_t encodeControl(const ControlFrame& frame, uint8_t* out, uint8_t capacity);

/// Parses a control frame. `out` is only modified when the result is `Ok`.
DecodeError decodeControl(const uint8_t* data, uint8_t length, ControlFrame& out);

/// Serialises `frame` into `out`. Same contract as encodeControl().
uint8_t encodeTelemetry(const TelemetryFrame& frame, uint8_t* out, uint8_t capacity);

/// Parses a telemetry frame. `out` is only modified when the result is `Ok`.
DecodeError decodeTelemetry(const uint8_t* data, uint8_t length, TelemetryFrame& out);

}  // namespace omnirc

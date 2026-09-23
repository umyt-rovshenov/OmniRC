#pragma once

#include <stdint.h>

namespace ctl {

/// Every failure in the firmware is reported as one of these. The firmware is
/// built without exceptions, so this enum plus Result<T> is the only error
/// channel; nothing is allowed to fail silently.
enum class Error : uint8_t {
    None = 0,
    Unknown,
    InvalidArgument,
    OutOfRange,
    NotInitialized,
    AlreadyInitialized,
    NotSupported,
    Timeout,
    Busy,
    NotFound,
    BufferTooSmall,
    HardwareFault,
    StorageFailure,
    ParseFailure,
    ValidationFailure,
    LinkDown,
};

/// Human readable name, for logs and on-screen messages. Never returns nullptr.
const char* toString(Error error);

}  // namespace ctl

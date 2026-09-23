#include "ctl/Error.h"

namespace ctl {

const char* toString(Error error) {
    switch (error) {
        case Error::None:
            return "none";
        case Error::Unknown:
            return "unknown";
        case Error::InvalidArgument:
            return "invalid argument";
        case Error::OutOfRange:
            return "out of range";
        case Error::NotInitialized:
            return "not initialized";
        case Error::AlreadyInitialized:
            return "already initialized";
        case Error::NotSupported:
            return "not supported";
        case Error::Timeout:
            return "timeout";
        case Error::Busy:
            return "busy";
        case Error::NotFound:
            return "not found";
        case Error::BufferTooSmall:
            return "buffer too small";
        case Error::HardwareFault:
            return "hardware fault";
        case Error::StorageFailure:
            return "storage failure";
        case Error::ParseFailure:
            return "parse failure";
        case Error::ValidationFailure:
            return "validation failure";
        case Error::LinkDown:
            return "link down";
    }
    return "unknown";
}

}  // namespace ctl

#include "ctl/Log.h"

#include <stdarg.h>
#include <stdio.h>

namespace ctl {
namespace log {
namespace {

/// Lines longer than this are truncated. Logging must never allocate, and a
/// truncated message is always better than a missed deadline.
constexpr int kMessageBufferSize = 192;

void defaultSink(Level level, const char* tag, const char* message) {
    printf("[%c][%s] %s\n", levelChar(level), tag, message);
}

Sink g_sink = &defaultSink;

}  // namespace

void setSink(Sink sink) {
    g_sink = (sink != nullptr) ? sink : &defaultSink;
}

char levelChar(Level level) {
    switch (level) {
        case Level::Error:
            return 'E';
        case Level::Warn:
            return 'W';
        case Level::Info:
            return 'I';
        case Level::Debug:
            return 'D';
        case Level::Verbose:
            return 'V';
        case Level::None:
            break;
    }
    return '?';
}

void write(Level level, const char* tag, const char* format, ...) {
    if (g_sink == nullptr || format == nullptr) {
        return;
    }

    char message[kMessageBufferSize];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    g_sink(level, (tag != nullptr) ? tag : "?", message);
}

}  // namespace log
}  // namespace ctl

#pragma once

#include <stdint.h>

/// Compile time filtered logging.
///
/// Levels above CTL_LOG_LEVEL are removed by the preprocessor, so a release
/// build pays nothing for verbose call sites: the arguments are never even
/// evaluated. Output goes through a sink function, which lets the host test
/// build print to stdout and the firmware print to UART0 without either one
/// knowing about the other.

#define CTL_LOG_LEVEL_NONE 0
#define CTL_LOG_LEVEL_ERROR 1
#define CTL_LOG_LEVEL_WARN 2
#define CTL_LOG_LEVEL_INFO 3
#define CTL_LOG_LEVEL_DEBUG 4
#define CTL_LOG_LEVEL_VERBOSE 5

#ifndef CTL_LOG_LEVEL
#define CTL_LOG_LEVEL CTL_LOG_LEVEL_INFO
#endif

namespace ctl {
namespace log {

enum class Level : uint8_t {
    None = CTL_LOG_LEVEL_NONE,
    Error = CTL_LOG_LEVEL_ERROR,
    Warn = CTL_LOG_LEVEL_WARN,
    Info = CTL_LOG_LEVEL_INFO,
    Debug = CTL_LOG_LEVEL_DEBUG,
    Verbose = CTL_LOG_LEVEL_VERBOSE,
};

/// Receives one fully formatted line. Must not block for long: it is called
/// from tasks that have real deadlines.
using Sink = void (*)(Level level, const char* tag, const char* message);

/// Installs the output sink. Passing nullptr restores the default, which
/// writes to stdout.
void setSink(Sink sink);

/// Formats and emits one line. Messages longer than the internal buffer are
/// truncated rather than allocating.
void write(Level level, const char* tag, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

/// Single letter used as the level marker in the default output.
char levelChar(Level level);

}  // namespace log
}  // namespace ctl

#if CTL_LOG_LEVEL >= CTL_LOG_LEVEL_ERROR
#define CTL_LOGE(tag, ...) ::ctl::log::write(::ctl::log::Level::Error, (tag), __VA_ARGS__)
#else
#define CTL_LOGE(tag, ...) ((void)0)
#endif

#if CTL_LOG_LEVEL >= CTL_LOG_LEVEL_WARN
#define CTL_LOGW(tag, ...) ::ctl::log::write(::ctl::log::Level::Warn, (tag), __VA_ARGS__)
#else
#define CTL_LOGW(tag, ...) ((void)0)
#endif

#if CTL_LOG_LEVEL >= CTL_LOG_LEVEL_INFO
#define CTL_LOGI(tag, ...) ::ctl::log::write(::ctl::log::Level::Info, (tag), __VA_ARGS__)
#else
#define CTL_LOGI(tag, ...) ((void)0)
#endif

#if CTL_LOG_LEVEL >= CTL_LOG_LEVEL_DEBUG
#define CTL_LOGD(tag, ...) ::ctl::log::write(::ctl::log::Level::Debug, (tag), __VA_ARGS__)
#else
#define CTL_LOGD(tag, ...) ((void)0)
#endif

#if CTL_LOG_LEVEL >= CTL_LOG_LEVEL_VERBOSE
#define CTL_LOGV(tag, ...) ::ctl::log::write(::ctl::log::Level::Verbose, (tag), __VA_ARGS__)
#else
#define CTL_LOGV(tag, ...) ((void)0)
#endif

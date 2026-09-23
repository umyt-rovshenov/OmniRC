/// Controllar transmitter firmware entry point.
///
/// Phase 0 only brings the toolchain up: it boots, reports what the hardware
/// looks like and proves that the shared protocol library links into the
/// firmware. Drivers and tasks arrive in the following phases.

#include <Arduino.h>

#include <controllar/Protocol.h>
#include <ctl/Log.h>
#include <ctl/Time.h>

namespace {

constexpr char kTag[] = "boot";

#ifndef CTL_FIRMWARE_VERSION
#define CTL_FIRMWARE_VERSION "0.0.0-dev"
#endif

/// Routes ctl::log output to UART0, which is exposed on the board's UART
/// USB-C port. The native USB peripheral stays free for the HID transport.
void serialSink(ctl::log::Level level, const char* tag, const char* message) {
    Serial.printf("[%8lu][%c][%s] %s\n", static_cast<unsigned long>(ctl::time::millis()),
                  ctl::log::levelChar(level), tag, message);
}

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return "power-on";
        case ESP_RST_EXT:
            return "external pin";
        case ESP_RST_SW:
            return "software";
        case ESP_RST_PANIC:
            return "panic";
        case ESP_RST_INT_WDT:
            return "interrupt watchdog";
        case ESP_RST_TASK_WDT:
            return "task watchdog";
        case ESP_RST_WDT:
            return "other watchdog";
        case ESP_RST_DEEPSLEEP:
            return "deep sleep wake";
        case ESP_RST_BROWNOUT:
            return "brownout";
        case ESP_RST_SDIO:
            return "sdio";
        default:
            return "unknown";
    }
}

/// Logs the machine state that matters when diagnosing a field problem. A
/// panic or brownout reset reason is the first thing worth knowing.
void logBootBanner() {
    CTL_LOGI(kTag, "Controllar transmitter %s", CTL_FIRMWARE_VERSION);
    CTL_LOGI(kTag, "built %s %s", __DATE__, __TIME__);
    CTL_LOGI(kTag, "reset reason: %s", resetReasonName(esp_reset_reason()));
    CTL_LOGI(kTag, "chip: %s rev %d, %d core(s) @ %lu MHz", ESP.getChipModel(),
             ESP.getChipRevision(), ESP.getChipCores(),
             static_cast<unsigned long>(getCpuFrequencyMhz()));
    CTL_LOGI(kTag, "flash: %lu KB", static_cast<unsigned long>(ESP.getFlashChipSize() / 1024));
    CTL_LOGI(kTag, "heap: %lu KB free", static_cast<unsigned long>(ESP.getFreeHeap() / 1024));

    const size_t psram = ESP.getPsramSize();
    if (psram == 0) {
        // N16R8 ships with 8 MB of octal PSRAM. Missing PSRAM means the build
        // flags do not match the board, and the UI will not fit later on.
        CTL_LOGE(kTag, "no PSRAM detected - check board_build.arduino.memory_type");
    } else {
        CTL_LOGI(kTag, "psram: %lu KB total, %lu KB free", static_cast<unsigned long>(psram / 1024),
                 static_cast<unsigned long>(ESP.getFreePsram() / 1024));
    }

    CTL_LOGI(kTag, "protocol v%u, up to %u channels, max frame %u bytes",
             controllar::kProtocolVersion, controllar::kMaxChannels,
             controllar::kMaxControlFrameSize);
}

ctl::time::Interval g_heartbeat(5000);

}  // namespace

void setup() {
    Serial.begin(115200);
    // Give a host terminal a moment to attach so the banner is not lost.
    ctl::time::delayMs(200);

    ctl::log::setSink(&serialSink);
    logBootBanner();
}

void loop() {
    if (g_heartbeat.expired()) {
        CTL_LOGD(kTag, "alive, uptime %lu s, heap %lu KB",
                 static_cast<unsigned long>(ctl::time::millis() / 1000),
                 static_cast<unsigned long>(ESP.getFreeHeap() / 1024));
    }
    ctl::time::delayMs(10);
}

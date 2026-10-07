/// OmniRC transmitter firmware entry point.
///
/// Phase 1 brings the hardware up one peripheral at a time. Each one gets a
/// self test that proves its wiring from the serial log and the screen, so a
/// fault is found while three wires are connected rather than forty.

#include <Arduino.h>

#include <ctl/Log.h>
#include <ctl/Time.h>
#include <ctl/hal/Display.h>
#include <ctl/hal/Pins.h>
#include <omnirc/Protocol.h>

namespace {

constexpr char kTag[] = "boot";

#ifndef CTL_FIRMWARE_VERSION
#define CTL_FIRMWARE_VERSION "0.0.0-dev"
#endif

ctl::hal::Display g_display;
ctl::time::Interval g_heartbeat(5000);

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
    CTL_LOGI(kTag, "OmniRC transmitter %s", CTL_FIRMWARE_VERSION);
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

    CTL_LOGI(kTag, "protocol v%u, up to %u channels, max frame %u bytes", omnirc::kProtocolVersion,
             omnirc::kMaxChannels, omnirc::kMaxControlFrameSize);
}

/// Draws a pattern whose every element answers one wiring question, so the
/// screen itself reports what is wrong instead of just staying dark.
///
/// Temporary scaffolding: the real interface replaces this in phase 4.
void drawDisplaySelfTest() {
    auto& gfx = g_display.gfx();

    // Primary colours in sequence. Wrong colours here mean rgb_order or
    // invert is set wrong for this panel, not that the wiring is bad.
    const uint16_t sequence[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
    const char* names[] = {"RED", "GREEN", "BLUE"};
    for (int i = 0; i < 3; ++i) {
        gfx.fillScreen(sequence[i]);
        gfx.setTextColor(TFT_WHITE);
        gfx.setTextSize(3);
        gfx.setCursor(60, 130);
        gfx.print(names[i]);
        ctl::time::delayMs(600);
    }

    gfx.fillScreen(TFT_BLACK);

    // A frame on the outermost pixels. If any edge is missing or the image
    // looks shifted, the panel offset is wrong rather than the wiring.
    gfx.drawRect(0, 0, ctl::hal::Display::kWidth, ctl::hal::Display::kHeight, TFT_WHITE);

    // Corner markers prove orientation and that no rows or columns are lost.
    const int kMarker = 16;
    gfx.fillRect(0, 0, kMarker, kMarker, TFT_RED);
    gfx.fillRect(ctl::hal::Display::kWidth - kMarker, 0, kMarker, kMarker, TFT_GREEN);
    gfx.fillRect(0, ctl::hal::Display::kHeight - kMarker, kMarker, kMarker, TFT_BLUE);
    gfx.fillRect(ctl::hal::Display::kWidth - kMarker, ctl::hal::Display::kHeight - kMarker, kMarker,
                 kMarker, TFT_YELLOW);

    gfx.setTextColor(TFT_WHITE);
    gfx.setTextSize(2);
    gfx.setCursor(14, 70);
    gfx.print("OmniRC");
    gfx.setTextSize(1);
    gfx.setCursor(14, 100);
    gfx.printf("firmware %s", CTL_FIRMWARE_VERSION);
    gfx.setCursor(14, 115);
    gfx.printf("panel %dx%d", ctl::hal::Display::kWidth, ctl::hal::Display::kHeight);
    gfx.setCursor(14, 130);
    gfx.print("display OK");

    gfx.setTextColor(TFT_DARKGREY);
    gfx.setCursor(14, 160);
    gfx.print("red    top left");
    gfx.setCursor(14, 172);
    gfx.print("green  top right");
    gfx.setCursor(14, 184);
    gfx.print("blue   bottom left");
    gfx.setCursor(14, 196);
    gfx.print("yellow bottom right");
}

}  // namespace

void setup() {
    Serial.begin(115200);
    // Give a host terminal a moment to attach so the banner is not lost.
    ctl::time::delayMs(200);

    ctl::log::setSink(&serialSink);
    logBootBanner();

    const ctl::Status display = g_display.begin();
    if (display.ok()) {
        drawDisplaySelfTest();
    } else {
        CTL_LOGE(kTag, "display unavailable: %s", ctl::toString(display.error()));
    }
}

void loop() {
    if (g_heartbeat.expired()) {
        CTL_LOGD(kTag, "alive, uptime %lu s, heap %lu KB, display %s",
                 static_cast<unsigned long>(ctl::time::millis() / 1000),
                 static_cast<unsigned long>(ESP.getFreeHeap() / 1024),
                 g_display.ready() ? "ok" : "down");
    }
    ctl::time::delayMs(10);
}

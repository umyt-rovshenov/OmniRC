#include "ctl/hal/Display.h"

#include "ctl/Log.h"
#include "ctl/hal/Pins.h"

namespace ctl {
namespace hal {
namespace {

constexpr char kTag[] = "display";

/// The panel accepts 40 MHz comfortably. A full frame is 240 x 280 x 2 bytes,
/// which at that clock is about 27 ms, so partial redraws are what keep the
/// interface responsive rather than raw bus speed.
constexpr uint32_t kSpiWriteHz = 40000000;

constexpr uint8_t kDefaultBrightness = 200;

}  // namespace

Display::Device::Device() {
    {
        auto cfg = m_bus.config();
        cfg.spi_host = SPI2_HOST;
        cfg.spi_mode = 0;
        cfg.freq_write = kSpiWriteHz;
        cfg.freq_read = 16000000;
        // The panel has no MISO line, so the bus runs in three wire mode.
        cfg.spi_3wire = true;
        cfg.use_lock = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk = pins::kTftSclk;
        cfg.pin_mosi = pins::kTftMosi;
        cfg.pin_miso = -1;
        cfg.pin_dc = pins::kTftDc;
        m_bus.config(cfg);
        m_panel.setBus(&m_bus);
    }

    {
        auto cfg = m_panel.config();
        cfg.pin_cs = pins::kTftCs;
        cfg.pin_rst = pins::kTftRst;
        cfg.pin_busy = -1;
        cfg.panel_width = Display::kWidth;
        cfg.panel_height = Display::kHeight;
        cfg.offset_x = 0;
        // A 240x280 panel sits 20 rows into the controller's 240x320 memory.
        cfg.offset_y = 20;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = false;
        cfg.invert = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = false;
        m_panel.config(cfg);
    }

    {
        auto cfg = m_light.config();
        cfg.pin_bl = pins::kTftBacklight;
        cfg.invert = false;
        cfg.freq = 12000;
        cfg.pwm_channel = 7;
        m_light.config(cfg);
        m_panel.setLight(&m_light);
    }

    setPanel(&m_panel);
}

Status Display::begin() {
    if (m_ready) {
        return Error::AlreadyInitialized;
    }

    if (!m_device.init()) {
        // The panel cannot be read back, so this only catches a failure to
        // claim the bus or the pins. A successful init with nothing visible
        // points at wiring instead.
        CTL_LOGE(kTag, "panel init failed");
        return Error::HardwareFault;
    }

    m_device.setRotation(0);
    m_device.fillScreen(TFT_BLACK);
    m_ready = true;
    setBrightness(kDefaultBrightness);

    CTL_LOGI(kTag, "ST7789 ready, %dx%d at %lu MHz", kWidth, kHeight,
             static_cast<unsigned long>(kSpiWriteHz / 1000000));
    return Status();
}

void Display::setBrightness(uint8_t level) {
    if (!m_ready) {
        return;
    }
    m_brightness = level;
    m_device.setBrightness(level);
}

}  // namespace hal
}  // namespace ctl

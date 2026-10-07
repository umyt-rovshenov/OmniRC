#pragma once

#include <LovyanGFX.hpp>
#include <stdint.h>

#include "ctl/Result.h"

namespace ctl {
namespace hal {

/// The 1.69 inch 240x280 ST7789 panel.
///
/// LovyanGFX is configured in code rather than through global build defines,
/// so the panel's wiring and quirks live here next to everything else that
/// knows about hardware, and `platformio.ini` stays readable.
class Display {
public:
    static constexpr int kWidth = 240;
    static constexpr int kHeight = 280;

    /// Brings up the bus, the panel and the backlight. The screen is cleared
    /// and the backlight raised before this returns, so a blank black screen
    /// after a successful call means a wiring fault rather than a slow start.
    Status begin();

    /// Backlight level, 0 to 255. Values are applied immediately.
    void setBrightness(uint8_t level);
    uint8_t brightness() const { return m_brightness; }

    bool ready() const { return m_ready; }

    /// Drawing surface. Only valid once begin() has succeeded.
    LGFX_Device& gfx() { return m_device; }

private:
    /// Panel and bus description. ST7789 controllers address a 240x320 frame
    /// buffer, so a 240x280 panel starts 20 rows in; without that offset the
    /// image is shifted and the bottom rows are lost. These panels are also
    /// wired with inverted colour.
    class Device : public lgfx::LGFX_Device {
    public:
        Device();

    private:
        lgfx::Panel_ST7789 m_panel;
        lgfx::Bus_SPI m_bus;
        lgfx::Light_PWM m_light;
    };

    Device m_device;
    uint8_t m_brightness = 0;
    bool m_ready = false;
};

}  // namespace hal
}  // namespace ctl

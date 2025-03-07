#pragma once

// only applicable on ESP32-S3
#ifdef USE_ESP32_VARIANT_ESP32S3
#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "driver/spi_master.h"

namespace esphome {
namespace gc9503v {

constexpr static const char *const TAG = "display.gc9503v";

class GC9503V : public display::Display {
  public:
    void update() override { this->do_update_(); }
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::HARDWARE; }
    void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                        display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;
    void draw_pixel_at(int x, int y, Color color) override;

    display::ColorOrder get_color_mode() { return this->color_mode_; }
    void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }
    void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }

    void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
    void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }
    void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
    void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
    void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
    void set_enable_pin(InternalGPIOPin *enable_pin) { this->enable_pin_ = enable_pin; }
    void set_reset_pin(InternalGPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
    void set_sclk_pin(InternalGPIOPin *sclk_pin) { this->sclk_pin_ = sclk_pin; }
    void set_mosi_pin(InternalGPIOPin *mosi_pin) { this->mosi_pin_ = mosi_pin; }
    void set_width(uint16_t width) { this->width_ = width; }
    void set_dimensions(uint16_t width, uint16_t height) {
      this->width_ = width;
      this->height_ = height;
    }
    int get_width() override;
    int get_height() override;
    void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
    void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
    void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
    void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
    void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
    void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
    void set_pclk_frequency(uint32_t pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
    void set_pclk_inverted(bool inverted) { this->pclk_inverted_ = inverted; }
    void set_offsets(int16_t offset_x, int16_t offset_y) {
      this->offset_x_ = offset_x;
      this->offset_y_ = offset_y;
    }
    display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
    void dump_config() override;
  protected:
    void __spi_send_cmd(uint8_t cmd);
    void __spi_send_data(uint8_t data);
    void rgb_driver_init();
    void rgb_driver_init_ha();
    
    int get_width_internal() override { return this->width_; }
    int get_height_internal() override { return this->height_; }
    void reset_display_() const;
    InternalGPIOPin *de_pin_{nullptr};
    InternalGPIOPin *pclk_pin_{nullptr};
    InternalGPIOPin *hsync_pin_{nullptr};
    InternalGPIOPin *vsync_pin_{nullptr};
    InternalGPIOPin *enable_pin_{nullptr};
    InternalGPIOPin *reset_pin_{nullptr};
    InternalGPIOPin *sclk_pin_{nullptr};
    InternalGPIOPin *mosi_pin_{nullptr};
    InternalGPIOPin *data_pins_[16] = {};
    uint16_t hsync_front_porch_ = 8;
    uint16_t hsync_pulse_width_ = 4;
    uint16_t hsync_back_porch_ = 8;
    uint16_t vsync_front_porch_ = 8;
    uint16_t vsync_pulse_width_ = 4;
    uint16_t vsync_back_porch_ = 8;
    uint32_t pclk_frequency_ = 16 * 1000 * 1000;
    bool pclk_inverted_{true};

    bool invert_colors_{};
    display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
    size_t width_{};
    size_t height_{};
    int16_t offset_x_{0};
    int16_t offset_y_{0};

    esp_lcd_panel_handle_t handle_{};
    spi_device_handle_t g_screen_spi{};
};

}  // namespace gc9503v
}  // namespace esphome
#endif
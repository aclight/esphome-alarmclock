// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2019 ESPHome
//
// Modified copy of esphome/components/mipi_rgb/mipi_rgb.h from ESPHome 2026.7.4.
// Changes vs upstream: configurable bounce_buffer_lines, an opt-out for the
// per-loop esp_lcd_rgb_panel_restart() call, and VSYNC/frame-complete counters
// used to measure DMA desyncs.
// See LICENSES/ESPHome-LICENSE.txt and components/mipi_rgb/LICENSE.

#pragma once

#if defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32P4)
#include <atomic>
#include <cstdint>
#include "esphome/core/gpio.h"
#include "esphome/components/display/display.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#ifdef USE_SPI
#include "esphome/components/spi/spi.h"
#endif

namespace esphome::mipi_rgb {

constexpr static const char *const TAG = "display.mipi_rgb";
const uint8_t SW_RESET_CMD = 0x01;
const uint8_t SLEEP_OUT = 0x11;
const uint8_t SDIR_CMD = 0xC7;
const uint8_t MADCTL_CMD = 0x36;
const uint8_t INVERT_OFF = 0x20;
const uint8_t INVERT_ON = 0x21;
const uint8_t DISPLAY_ON = 0x29;
const uint8_t CMD2_BKSEL = 0xFF;
const uint8_t CMD2_BK0[5] = {0x77, 0x01, 0x00, 0x00, 0x10};

class MipiRgb : public display::Display {
 public:
  MipiRgb(int width, int height) : width_(width), height_(height) {}
  void setup() override;
  void loop() override;
  void update() override;
  void fill(Color color) override;
  void draw_pixels_at(int x_start, int y_start, int w, int h, const uint8_t *ptr, display::ColorOrder order,
                      display::ColorBitness bitness, bool big_endian, int x_offset, int y_offset, int x_pad) override;

  display::ColorOrder get_color_mode() { return this->color_mode_; }
  void set_color_mode(display::ColorOrder color_mode) { this->color_mode_ = color_mode; }
  void set_invert_colors(bool invert_colors) { this->invert_colors_ = invert_colors; }

  void add_data_pin(InternalGPIOPin *data_pin, size_t index) { this->data_pins_[index] = data_pin; };
  void set_de_pin(InternalGPIOPin *de_pin) { this->de_pin_ = de_pin; }
  void set_pclk_pin(InternalGPIOPin *pclk_pin) { this->pclk_pin_ = pclk_pin; }
  void set_vsync_pin(InternalGPIOPin *vsync_pin) { this->vsync_pin_ = vsync_pin; }
  void set_hsync_pin(InternalGPIOPin *hsync_pin) { this->hsync_pin_ = hsync_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_width(uint16_t width) { this->width_ = width; }
  void set_pclk_frequency(uint32_t pclk_frequency) { this->pclk_frequency_ = pclk_frequency; }
  void set_pclk_inverted(bool inverted) { this->pclk_inverted_ = inverted; }
  void set_bounce_buffer_lines(uint16_t lines) { this->bounce_buffer_lines_ = lines; }
  void set_force_restart(bool force_restart) { this->force_restart_ = force_restart; }
  void set_desync_report_interval(uint32_t ms) { this->desync_report_interval_ = ms; }
  void set_late_frame_threshold(uint32_t us) { this->late_frame_threshold_ = us; }
  void set_model(const char *model) { this->model_ = model; }
  int get_width() override;
  int get_height() override;
  void set_hsync_back_porch(uint16_t hsync_back_porch) { this->hsync_back_porch_ = hsync_back_porch; }
  void set_hsync_front_porch(uint16_t hsync_front_porch) { this->hsync_front_porch_ = hsync_front_porch; }
  void set_hsync_pulse_width(uint16_t hsync_pulse_width) { this->hsync_pulse_width_ = hsync_pulse_width; }
  void set_vsync_pulse_width(uint16_t vsync_pulse_width) { this->vsync_pulse_width_ = vsync_pulse_width; }
  void set_vsync_back_porch(uint16_t vsync_back_porch) { this->vsync_back_porch_ = vsync_back_porch; }
  void set_vsync_front_porch(uint16_t vsync_front_porch) { this->vsync_front_porch_ = vsync_front_porch; }
  void set_enable_pins(std::vector<GPIOPin *> enable_pins) { this->enable_pins_ = std::move(enable_pins); }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return this->width_; }
  int get_height_internal() override { return this->height_; }
  void dump_config() override;
  void draw_pixel_at(int x, int y, Color color) override;

  // this will be horribly slow.
 protected:
  void write_to_display_(int x_start, int y_start, int w, int h, const uint8_t *ptr, int x_offset, int y_offset,
                         int x_pad);
  bool check_buffer_();
  void dump_pins_(uint8_t start, uint8_t end, const char *name, uint8_t offset);
  void setup_enables_();
  void common_setup_();
  void report_desync_();
  void drain_late_frames_();

  // Both fire once per frame: on_vsync from the hardware VSYNC_END interrupt,
  // on_frame_buf_complete when the driver's software bounce position wraps.
  // Any divergence between the two counts is a DMA desync.
  static bool vsync_cb_(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
  static bool frame_complete_cb_(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata,
                                 void *user_ctx);

  // Hardware VSYNC is exactly periodic, so jitter in the timestamp taken inside
  // the callback is interrupt latency - the condition ESP-IDF blames for the
  // single-frame shift its own DMA restart can cause.
  struct LateFrame {
    uint32_t interval_us;
    uint32_t slack_us;
  };
  static constexpr uint8_t LATE_EVENT_SLOTS = 16;

  std::atomic<uint32_t> vsync_count_{0};
  std::atomic<uint32_t> frame_complete_count_{0};
  std::atomic<uint32_t> late_frame_count_{0};
  std::atomic<uint32_t> late_dropped_{0};
  std::atomic<uint32_t> max_interval_us_{0};
  std::atomic<uint32_t> min_slack_us_{UINT32_MAX};
  std::atomic<uint8_t> late_write_{0};
  std::atomic<uint8_t> late_read_{0};
  LateFrame late_events_[LATE_EVENT_SLOTS]{};
  // Only ever touched from the single LCD ISR, so no synchronisation needed.
  int64_t last_vsync_us_{0};
  int64_t last_fb_complete_us_{0};
  uint32_t frame_period_us_{0};
  uint32_t late_threshold_us_{0};
  uint32_t late_frame_threshold_{200};
  uint32_t last_vsync_count_{0};
  uint32_t last_frame_complete_count_{0};
  int32_t last_drift_{0};
  uint32_t total_desyncs_{0};
  uint32_t last_report_ms_{0};
  uint32_t desync_report_interval_{0};
  bool force_restart_{true};
  InternalGPIOPin *de_pin_{nullptr};
  InternalGPIOPin *pclk_pin_{nullptr};
  InternalGPIOPin *hsync_pin_{nullptr};
  InternalGPIOPin *vsync_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  InternalGPIOPin *data_pins_[16] = {};
  uint16_t hsync_pulse_width_ = 10;
  uint16_t hsync_back_porch_ = 10;
  uint16_t hsync_front_porch_ = 20;
  uint16_t vsync_pulse_width_ = 10;
  uint16_t vsync_back_porch_ = 10;
  uint16_t vsync_front_porch_ = 10;
  uint32_t pclk_frequency_ = 16 * 1000 * 1000;
  // Scanlines per bounce buffer; two of these are allocated in internal SRAM.
  uint16_t bounce_buffer_lines_{10};
  bool pclk_inverted_{true};
  const char *model_{"Unknown"};
  bool invert_colors_{};
  display::ColorOrder color_mode_{display::COLOR_ORDER_BGR};
  size_t width_;
  size_t height_;
  uint16_t *buffer_{nullptr};
  std::vector<GPIOPin *> enable_pins_{};
  uint16_t x_low_{1};
  uint16_t y_low_{1};
  uint16_t x_high_{0};
  uint16_t y_high_{0};

  esp_lcd_panel_handle_t handle_{};
};

#ifdef USE_SPI
class MipiRgbSpi final : public MipiRgb,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  MipiRgbSpi(int width, int height) : MipiRgb(width, height) {}

  void set_init_sequence(const std::vector<uint8_t> &init_sequence) { this->init_sequence_ = init_sequence; }
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void setup() override;

 protected:
  void write_command_(uint8_t value);
  void write_data_(uint8_t value);
  void write_init_sequence_();
  void dump_config() override;

  GPIOPin *dc_pin_{nullptr};
  std::vector<uint8_t> init_sequence_;
};
#endif

}  // namespace esphome::mipi_rgb
#endif

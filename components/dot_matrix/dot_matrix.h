#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/spi/spi.h"

#include <string>

namespace esphome {
namespace dot_matrix {

static const uint8_t MAX_CASCADE_SIZE = 16;
static const uint8_t MAX_BRIGHTNESS = 15;

// MAX7219 driven over the shared ESPHome SPI bus. Rendering (font, flexible
// character widths, UTF-8 Norwegian letters, scrolling) is ported from the
// original DotMatrix MicroPython c-module; only the transport and the binding
// layer are new.
class DotMatrix : public Component,
                  public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                        spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  // --- configuration (called from codegen) ---
  void set_num_modules(uint8_t n) { this->num_modules_ = n; }
  void set_scroll_delay(uint16_t ms) { this->scroll_delay_ = ms; }
  void set_brightness(uint8_t b);  // 0..15, also applies live if running

  // --- public API mirroring the original library ---
  // Draw static text once. Optionally centred on the display.
  void write(const std::string &text, bool centered = false);
  // Start scrolling text; advanced from loop(). wrap=true repeats forever.
  void marquee(const std::string &text, bool wrap = true);
  // Clear the display and stop any scrolling.
  void clear();

 protected:
  // transport
  void send_(uint8_t chip, uint16_t value);
  void init_display_();
  void display_set_segment_(uint8_t digit, uint8_t val);
  void display_clear_();

  // rendering (ported from matrix.c)
  uint8_t get_char_column_(uint8_t chr, uint8_t pos);
  static size_t text_length_(const char *text);
  void copy_text_(const char *text, bool center);
  bool scroll_buffer_();
  void print_buffer_();
  static void rotate_ccw_(const uint8_t *frame, uint8_t *rotated);

  uint8_t num_modules_{8};
  uint8_t digits_{0};
  uint8_t brightness_{0};
  bool mirrored_{false};

  uint8_t buffer_[MAX_CASCADE_SIZE * 8] = {0};

  // scrolling state
  bool scrolling_{false};
  bool wrap_{true};
  std::string scroll_text_;
  uint16_t text_index_{0};
  uint16_t col_index_{0};
  int16_t scroll_whitespace_{0};
  uint16_t scroll_delay_{30};
  uint32_t scroll_ts_{0};
};

// --------------------------- Actions ---------------------------------------

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DotMatrix> {
 public:
  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(bool, centered)
  void play(Ts... x) override {
    this->parent_->write(this->text_.value(x...), this->centered_.value(x...));
  }
};

template<typename... Ts> class MarqueeAction : public Action<Ts...>, public Parented<DotMatrix> {
 public:
  TEMPLATABLE_VALUE(std::string, text)
  TEMPLATABLE_VALUE(bool, wrap)
  void play(Ts... x) override {
    this->parent_->marquee(this->text_.value(x...), this->wrap_.value(x...));
  }
};

template<typename... Ts> class ClearAction : public Action<Ts...>, public Parented<DotMatrix> {
 public:
  void play(Ts... x) override { this->parent_->clear(); }
};

}  // namespace dot_matrix
}  // namespace esphome

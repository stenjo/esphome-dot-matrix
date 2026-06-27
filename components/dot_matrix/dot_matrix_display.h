#pragma once

#include "esphome/core/defines.h"
#ifdef USE_DISPLAY

#include "esphome/core/component.h"
#include "esphome/core/color.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

#include <functional>
#include <vector>

namespace esphome {
namespace dot_matrix {

// A standard ESPHome display platform for cascaded 8x8 MAX7219 panels.
//
// Works with the normal display API (it.print / it.printf / it.strftime /
// it.line ... using ESPHome `font:` definitions) AND exposes print_dm(), which
// renders text with the ported flexible-width Norwegian font from
// dot_matrix_font.h. The two can be mixed freely in the same lambda.
class DotMatrixDisplay : public display::DisplayBuffer,
                         public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                               spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1MHZ> {
 public:
  void set_writer(std::function<void(DotMatrixDisplay &)> &&writer) { this->writer_ = std::move(writer); }
  void set_num_modules(uint8_t n) { this->num_modules_ = n; }
  void set_intensity(uint8_t i) { this->intensity_ = i; }

  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return this->num_modules_ * 8; }
  int get_height_internal() override { return 8; }
  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_BINARY; }

  // Live brightness change (0..15).
  void intensity(uint8_t value);

  // Draw text with the bundled flexible-width Norwegian font, starting at
  // pixel column x_start. UTF-8 Æ/Ø/Å/æ/ø/å render directly. Returns the next
  // free column so calls can be chained.
  int print_dm(int x_start, const char *text, Color color = display::COLOR_ON);
  int print_dm(int x_start, const std::string &text, Color color = display::COLOR_ON) {
    return this->print_dm(x_start, text.c_str(), color);
  }

  // Pixel width print_dm() would occupy (inter-glyph gaps included, trailing
  // gap excluded). Does not draw anything.
  int measure_dm(const char *text);
  int measure_dm(const std::string &text) { return this->measure_dm(text.c_str()); }

  // Draw text centered across the full display width (mirrors the original
  // write(text, centered=true)). Returns the column after the last glyph.
  int print_dm_centered(const char *text, Color color = display::COLOR_ON);
  int print_dm_centered(const std::string &text, Color color = display::COLOR_ON) {
    return this->print_dm_centered(text.c_str(), color);
  }

 protected:
  void send_byte_(uint8_t reg, uint8_t data);
  void send_to_all_(uint8_t reg, uint8_t data);
  void send_(uint8_t chip, uint8_t reg, uint8_t data);
  void init_display_();
  void flush_();
  static void rotate_ccw_(const uint8_t *frame, uint8_t *rotated);

  uint8_t num_modules_{8};
  uint8_t intensity_{3};
  std::vector<uint8_t> buffer_;  // column-major: buffer_[x], bit y = pixel(x,y)
  std::function<void(DotMatrixDisplay &)> writer_{};
};

}  // namespace dot_matrix
}  // namespace esphome

#endif  // USE_DISPLAY

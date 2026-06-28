#include "esphome/core/defines.h"
#ifdef USE_DISPLAY
#include "dot_matrix_display.h"
#include "dot_matrix_font.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstring>

namespace esphome {
namespace dot_matrix {

static const char *const TAG = "dot_matrix.display";

static const uint8_t REG_NOOP = 0x00;
static const uint8_t REG_DIGIT_0 = 0x01;
static const uint8_t REG_DECODE_MODE = 0x09;
static const uint8_t REG_INTENSITY = 0x0A;
static const uint8_t REG_SCAN_LIMIT = 0x0B;
static const uint8_t REG_SHUTDOWN = 0x0C;
static const uint8_t REG_DISPLAY_TEST = 0x0F;

// --------------------------------------------------------------------------
//  Lifecycle
// --------------------------------------------------------------------------

void DotMatrixDisplay::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DotMatrix display...");
  this->spi_setup();
  this->buffer_.assign(this->get_width_internal(), 0);
  this->init_display_();
}

void DotMatrixDisplay::update() {
  std::fill(this->buffer_.begin(), this->buffer_.end(), 0);
  if (this->writer_)
    this->writer_(*this);
  this->flush_();
}

void DotMatrixDisplay::dump_config() {
  ESP_LOGCONFIG(TAG, "DotMatrix display:");
  ESP_LOGCONFIG(TAG, "  Modules: %u (cascaded 8x8)", this->num_modules_);
  ESP_LOGCONFIG(TAG, "  Intensity: %u/15", this->intensity_);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

// --------------------------------------------------------------------------
//  ESPHome DisplayBuffer integration
// --------------------------------------------------------------------------

void DotMatrixDisplay::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= this->get_width_internal() || y < 0 || y >= 8)
    return;
  if (color.is_on())
    this->buffer_[x] |= (1 << y);
  else
    this->buffer_[x] &= ~(1 << y);
}

// --------------------------------------------------------------------------
//  Flexible-width Norwegian font (ported from matrix.c / copyText)
// --------------------------------------------------------------------------

int DotMatrixDisplay::print_dm(int x_start, const char *text, Color color) {
  int cx = x_start;
  size_t slen = std::strlen(text);
  for (size_t i = 0; i < slen; i++) {
    uint8_t chr = text[i];

    if (chr == ' ') {
      cx += 3;  // matches the original 2-3 column space
      continue;
    }
    if (chr == ESCAPE_CHAR || chr == ESCAPE_CHAR_2) {  // 0xC3 Æ/Ø/Å/æ/ø/å, 0xC2 °
      if (++i >= slen)
        break;
      chr = text[i];
    }
    if (chr < 32 || (uint8_t) (chr - 32) >= FONT_GLYPH_COUNT)
      continue;

    uint16_t idx = FONT_INDEX[chr - 32];
    uint8_t width = FONT[idx];
    for (uint8_t c = 0; c < width; c++) {
      uint8_t colb = FONT[idx + 1 + c];
      for (uint8_t y = 0; y < 8; y++)
        if (colb & (1 << y))
          this->draw_pixel_at(cx, y, color);
      cx++;
    }
    cx++;  // 1 column gap between glyphs
  }
  return cx;
}

int DotMatrixDisplay::measure_dm(const char *text) {
  int cx = 0;
  size_t slen = std::strlen(text);
  for (size_t i = 0; i < slen; i++) {
    uint8_t chr = text[i];
    if (chr == ' ') {
      cx += 3;
      continue;
    }
    if (chr == ESCAPE_CHAR || chr == ESCAPE_CHAR_2) {
      if (++i >= slen)
        break;
      chr = text[i];
    }
    if (chr < 32 || (uint8_t) (chr - 32) >= FONT_GLYPH_COUNT)
      continue;
    cx += FONT[FONT_INDEX[chr - 32]] + 1;  // glyph width + inter-glyph gap
  }
  return cx > 0 ? cx - 1 : 0;  // drop the trailing gap
}

int DotMatrixDisplay::print_dm_centered(const char *text, Color color) {
  int start = (this->get_width() - this->measure_dm(text)) / 2;
  if (start < 0)
    start = 0;
  return this->print_dm(start, text, color);
}

// --------------------------------------------------------------------------
//  SPI transport + flush
// --------------------------------------------------------------------------

void DotMatrixDisplay::send_byte_(uint8_t reg, uint8_t data) {
  this->write_byte(reg);
  this->write_byte(data);
}

void DotMatrixDisplay::send_to_all_(uint8_t reg, uint8_t data) {
  this->enable();
  for (uint8_t i = 0; i < this->num_modules_; i++)
    this->send_byte_(reg, data);
  this->disable();
}

// Address a single chip in the daisy chain (others get NOOP).
void DotMatrixDisplay::send_(uint8_t chip, uint8_t reg, uint8_t data) {
  this->enable();
  for (uint8_t i = 0; i < this->num_modules_; i++) {
    if (i == chip)
      this->send_byte_(reg, data);
    else
      this->send_byte_(REG_NOOP, REG_NOOP);
  }
  this->disable();
}

void DotMatrixDisplay::init_display_() {
  this->send_to_all_(REG_SHUTDOWN, 0);                 // shutdown
  this->send_to_all_(REG_DISPLAY_TEST, 0);             // test off
  this->send_to_all_(REG_SCAN_LIMIT, 7);               // scan all 8 rows
  this->send_to_all_(REG_DECODE_MODE, 0);              // no BCD decode
  this->send_to_all_(REG_INTENSITY, this->intensity_); // brightness
  this->send_to_all_(REG_SHUTDOWN, 1);                 // wake up
}

void DotMatrixDisplay::intensity(uint8_t value) {
  if (value > 15)
    value = 15;
  this->intensity_ = value;
  this->send_to_all_(REG_INTENSITY, value);
}

void DotMatrixDisplay::rotate_ccw_(const uint8_t *frame, uint8_t *rotated) {
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 8; ++j)
      if (frame[i] & (1 << j))
        rotated[j] |= (1 << (7 - i));
}

void DotMatrixDisplay::flush_() {
  uint8_t rows[8];
  for (uint8_t d = 0; d < this->num_modules_; d++) {
    std::memset(rows, 0, 8);
    this->rotate_ccw_(&this->buffer_[d * 8], rows);
    // Write the 8 digit registers of chip d (one row each).
    for (uint8_t r = 0; r < 8; r++)
      this->send_(d, REG_DIGIT_0 + r, rows[r]);
  }
}

}  // namespace dot_matrix
}  // namespace esphome

#endif  // USE_DISPLAY

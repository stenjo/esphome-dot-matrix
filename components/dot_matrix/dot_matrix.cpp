#include "dot_matrix.h"
#include "dot_matrix_font.h"
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace dot_matrix {

static const char *const TAG = "dot_matrix";

// MAX7219 registers (high byte of the 16-bit word)
static const uint16_t REG_DIGIT_0 = (1 << 8);
static const uint16_t REG_DECODE_MODE = (9 << 8);
static const uint16_t REG_INTENSITY = (10 << 8);
static const uint16_t REG_SCAN_LIMIT = (11 << 8);
static const uint16_t REG_SHUTDOWN = (12 << 8);
static const uint16_t REG_DISPLAY_TEST = (15 << 8);

static const uint8_t ALL_CHIPS = 0xff;
static const uint8_t ALL_DIGITS = 8;

// --------------------------------------------------------------------------
//  Component lifecycle
// --------------------------------------------------------------------------

void DotMatrix::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DotMatrix...");
  this->spi_setup();
  this->digits_ = this->num_modules_ * ALL_DIGITS;
  this->init_display_();
  this->display_clear_();
}

void DotMatrix::loop() {
  if (!this->scrolling_)
    return;

  uint32_t now = millis();
  if ((now < this->scroll_ts_) || (now - this->scroll_ts_ < this->scroll_delay_))
    return;
  this->scroll_ts_ = now + this->scroll_delay_;

  bool done = this->scroll_buffer_();
  this->print_buffer_();
  if (done && !this->wrap_)
    this->scrolling_ = false;
}

void DotMatrix::dump_config() {
  ESP_LOGCONFIG(TAG, "DotMatrix:");
  ESP_LOGCONFIG(TAG, "  Modules: %u (cascaded 8x8)", this->num_modules_);
  ESP_LOGCONFIG(TAG, "  Brightness: %u/15", this->brightness_);
  ESP_LOGCONFIG(TAG, "  Scroll delay: %u ms", this->scroll_delay_);
  LOG_PIN("  CS Pin: ", this->cs_);
}

// --------------------------------------------------------------------------
//  Public API
// --------------------------------------------------------------------------

void DotMatrix::write(const std::string &text, bool centered) {
  this->scrolling_ = false;
  this->copy_text_(text.c_str(), centered);
}

void DotMatrix::marquee(const std::string &text, bool wrap) {
  this->scroll_text_ = text;
  this->wrap_ = wrap;
  this->text_index_ = 0;
  this->col_index_ = 0;
  this->scroll_whitespace_ = 0;
  this->scroll_ts_ = 0;
  std::memset(this->buffer_, 0, sizeof(this->buffer_));
  this->display_clear_();
  this->scrolling_ = true;
}

void DotMatrix::clear() {
  this->scrolling_ = false;
  std::memset(this->buffer_, 0, sizeof(this->buffer_));
  this->display_clear_();
}

void DotMatrix::set_brightness(uint8_t b) {
  if (b > MAX_BRIGHTNESS)
    b = MAX_BRIGHTNESS;
  this->brightness_ = b;
  if (this->digits_ != 0)  // already set up -> apply live
    this->send_(ALL_CHIPS, REG_INTENSITY | b);
}

// --------------------------------------------------------------------------
//  SPI transport (replaces the ESP-IDF spi_master driver in max7219.c)
// --------------------------------------------------------------------------

void DotMatrix::send_(uint8_t chip, uint16_t value) {
  this->enable();
  for (uint8_t i = 0; i < this->num_modules_; i++) {
    uint16_t word = (chip == ALL_CHIPS || i == chip) ? value : 0;
    this->write_byte(word >> 8);    // register
    this->write_byte(word & 0xff);  // data
  }
  this->disable();
}

void DotMatrix::init_display_() {
  if (!this->num_modules_ || this->num_modules_ > MAX_CASCADE_SIZE) {
    ESP_LOGE(TAG, "Invalid module count %u", this->num_modules_);
    return;
  }
  this->send_(ALL_CHIPS, REG_SHUTDOWN | 0);                     // shutdown
  this->send_(ALL_CHIPS, REG_DISPLAY_TEST | 0);                 // test off
  this->send_(ALL_CHIPS, REG_SCAN_LIMIT | (ALL_DIGITS - 1));    // scan all 8
  this->send_(ALL_CHIPS, REG_DECODE_MODE | 0);                  // no BCD decode
  this->send_(ALL_CHIPS, REG_INTENSITY | this->brightness_);    // brightness
  this->send_(ALL_CHIPS, REG_SHUTDOWN | 1);                     // wake up
}

void DotMatrix::display_set_segment_(uint8_t digit, uint8_t val) {
  if (digit >= this->digits_)
    return;
  if (this->mirrored_)
    digit = this->digits_ - digit - 1;
  uint8_t c = digit / ALL_DIGITS;
  uint8_t d = digit % ALL_DIGITS;
  this->send_(c, (REG_DIGIT_0 + ((uint16_t) d << 8)) | val);
}

void DotMatrix::display_clear_() {
  for (uint8_t i = 0; i < ALL_DIGITS; i++)
    this->send_(ALL_CHIPS, (REG_DIGIT_0 + ((uint16_t) i << 8)) | 0);
}

// --------------------------------------------------------------------------
//  Rendering core (ported verbatim from matrix.c)
// --------------------------------------------------------------------------

void DotMatrix::rotate_ccw_(const uint8_t *frame, uint8_t *rotated) {
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 8; ++j)
      if (frame[i] & (1 << j))
        rotated[j] |= (1 << (7 - i));
}

uint8_t DotMatrix::get_char_column_(uint8_t chr, uint8_t pos) {
  uint8_t asc = chr - 32;
  uint16_t idx = FONT_INDEX[asc];
  uint8_t w = FONT[idx];
  if (pos >= w)
    return 0;
  return FONT[pos + idx + 1];
}

size_t DotMatrix::text_length_(const char *text) {
  size_t len = 0;
  size_t n = std::strlen(text);
  for (uint16_t i = 0; i < n; i++) {
    if ((uint8_t)(text[i]) == ESCAPE_CHAR)
      i++;
    len += FONT[FONT_INDEX[(uint8_t)(text[i]) - 32]] + 1;
  }
  return len - 1;
}

void DotMatrix::copy_text_(const char *text, bool center) {
  size_t txt_len = text_length_(text);
  int16_t buffer_index = 0;
  int16_t padding = 0;
  const int16_t cap = this->num_modules_ * 8;

  if (txt_len < (size_t) cap && center) {
    padding = (cap - txt_len) / 2;
    for (int16_t i = 0; i < padding; i++)
      this->buffer_[buffer_index++] = 0;
  }

  size_t slen = std::strlen(text);
  for (size_t str_idx = 0; str_idx < slen && buffer_index < cap; str_idx++) {
    uint8_t chr = text[str_idx];

    if (chr == ' ') {
      this->buffer_[buffer_index++] = 0;
      if (buffer_index < cap)
        this->buffer_[buffer_index++] = 0;
      if (buffer_index < cap)
        this->buffer_[buffer_index++] = 0;
      continue;
    }

    if (chr == ESCAPE_CHAR) {
      str_idx++;
      if (str_idx < slen) {
        chr = text[str_idx];
      } else {
        break;
      }
    }

    for (int16_t col_idx = 0;; col_idx++) {
      uint8_t col = this->get_char_column_(chr, col_idx);
      if (col == 0 || buffer_index >= cap)
        break;
      this->buffer_[buffer_index++] = col;
    }
    if (buffer_index < cap)
      this->buffer_[buffer_index++] = 0;
  }

  while (buffer_index < cap)
    this->buffer_[buffer_index++] = 0;

  this->print_buffer_();
}

bool DotMatrix::scroll_buffer_() {
  const char *text = this->scroll_text_.c_str();
  size_t buffer_size = this->num_modules_ * 8;
  size_t text_length = this->scroll_text_.size();

  std::memmove(this->buffer_, this->buffer_ + 1, buffer_size - 1);

  if (this->text_index_ >= text_length) {
    this->buffer_[buffer_size - 1] = 0;
    if (--this->scroll_whitespace_ <= 0) {
      if (this->wrap_) {
        this->text_index_ = 0;
        this->col_index_ = 0;
        this->scroll_whitespace_ = buffer_size;
      }
      return true;
    }
  } else {
    uint8_t chr = text[this->text_index_];
    uint8_t col = 0;

    if (chr == ' ' && this->col_index_ < 1) {
      this->col_index_++;
      return false;
    } else {
      col = this->get_char_column_(chr, this->col_index_);
      if (chr == ESCAPE_CHAR) {
        chr = text[++this->text_index_];
        col = this->get_char_column_(chr, this->col_index_);
      }
    }

    if (col != 0) {
      this->buffer_[buffer_size - 1] = col;
      this->col_index_++;
    } else {
      this->buffer_[buffer_size - 1] = 0;
      this->col_index_ = 0;
      this->text_index_++;
    }
    this->scroll_whitespace_ = buffer_size;
  }
  return false;
}

void DotMatrix::print_buffer_() {
  uint8_t block[8];
  for (int16_t d = 0; d < this->num_modules_; d++) {
    std::memset(block, 0, 8);
    this->rotate_ccw_(this->buffer_ + d * 8, block);
    for (uint8_t i = 0; i < 8; i++)
      this->display_set_segment_(d * 8 + i, block[i]);
  }
}

}  // namespace dot_matrix
}  // namespace esphome

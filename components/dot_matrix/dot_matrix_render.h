#pragma once

#include "dot_matrix_font.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace dot_matrix {
namespace render {

inline size_t measure_text(const char *text) {
  size_t length = 0;
  size_t size = std::strlen(text);
  bool trailing_glyph_gap = false;
  for (size_t i = 0; i < size; i++) {
    uint8_t chr = text[i];
    if (chr == ' ') {
      length += 3;
      trailing_glyph_gap = false;
      continue;
    }
    if (chr == ESCAPE_CHAR || chr == ESCAPE_CHAR_2) {
      if (++i >= size)
        break;
      chr = text[i];
    }
    if (chr < 32 || (uint8_t) (chr - 32) >= FONT_GLYPH_COUNT)
      continue;
    length += FONT[FONT_INDEX[chr - 32]] + 1;
    trailing_glyph_gap = true;
  }
  return trailing_glyph_gap ? length - 1 : length;
}

inline void rotate_ccw(const uint8_t *frame, uint8_t *rotated) {
  for (int i = 0; i < 8; ++i)
    for (int j = 0; j < 8; ++j)
      if (frame[i] & (1 << j))
        rotated[j] |= (1 << (7 - i));
}

}  // namespace render
}  // namespace dot_matrix
}  // namespace esphome
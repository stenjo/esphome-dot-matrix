#include "dot_matrix_render.h"

#include <cassert>
#include <cstdint>
#include <cstring>

using esphome::dot_matrix::render::measure_text;
using esphome::dot_matrix::render::rotate_ccw;

int main() {
  assert(measure_text("") == 0);
  assert(measure_text("A") == 5);
  assert(measure_text("A B") == 14);
  assert(measure_text(" ") == 3);
  assert(measure_text("A ") == 9);
  assert(measure_text("\xC3\x85") == 5);  // Å
  assert(measure_text("\xC2\xB0") == 3);  // °
  assert(measure_text("A\x01" "B") == 11);  // unsupported control byte
  assert(measure_text("A\xC3") == 5);     // truncated UTF-8 sequence

  for (int row = 0; row < 8; row++) {
    for (int column = 0; column < 8; column++) {
      uint8_t frame[8] = {};
      uint8_t rotated[8] = {};
      frame[row] = 1 << column;
      rotate_ccw(frame, rotated);

      for (int output_row = 0; output_row < 8; output_row++) {
        uint8_t expected = output_row == column ? 1 << (7 - row) : 0;
        assert(rotated[output_row] == expected);
      }
    }
  }
}
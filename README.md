# esphome-dot-matrix

[![CI](https://github.com/stenjo/esphome-dot-matrix/actions/workflows/ci.yml/badge.svg)](https://github.com/stenjo/esphome-dot-matrix/actions/workflows/ci.yml)

An [ESPHome](https://esphome.io/) external component for MAX7219-based
cascaded 8×8 LED dot-matrix displays, with rendering ported from the original
[`dot-matrix-calendar`](https://github.com/stenjo/dot-matrix-calendar)
MicroPython c-module.

Why a port instead of the built-in `max7219digit`? This keeps the original
rendering engine: **flexible per-character widths** (no fixed cells) and a
built-in font with **Norwegian letters in both cases** (Æ Ø Å æ ø å). Because
those glyphs are placed at the byte values of their UTF-8 continuation bytes
(with `0xC3` used as an escape), plain UTF-8 text from your YAML renders
correctly with no transcoding.

## Installation

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/stenjo/esphome-dot-matrix
      ref: main
    components: [dot_matrix]
```

During development you can point at a local checkout instead:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [dot_matrix]
```

## Configuration

```yaml
spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11

dot_matrix:
  id: display1
  cs_pin: GPIO10
  num_modules: 8     # cascaded 8x8 panels (1..16)
  brightness: 3      # 0..15
  scroll_delay: 30ms # marquee step interval
```

The component is an SPI device on the shared `spi:` bus, so it coexists with
other SPI peripherals. MAX7219 only needs CLK, MOSI (DIN) and CS — no MISO.

## Actions

| Action | Parameters | Notes |
| --- | --- | --- |
| `dot_matrix.write` | `text` (templatable), `centered` (bool, default `false`) | Draws static text once. |
| `dot_matrix.marquee` | `text` (templatable), `wrap` (bool, default `true`) | Scrolls text; advanced from `loop()`. |
| `dot_matrix.clear` | — | Clears the display and stops scrolling. |

```yaml
on_...:
  - dot_matrix.write:
      id: display1
      text: "Tirsdag 27. mai"
      centered: true
  - dot_matrix.marquee:
      id: display1
      text: !lambda 'return id(my_sensor).state > 0 ? "Blåbær" : "Tomt";'
```

You can also call the C++ API directly from a lambda:
`id(display1).write("Hei");`, `id(display1).marquee("Rull", true);`,
`id(display1).clear();`.

## Display platform (`it.print()` style)

In addition to the action-based component above, the repo provides a standard
ESPHome **display platform**. Use this when you want a dashboard-style display
that re-renders on every `update_interval` and mixes in sensor values, a clock,
lines, etc.

```yaml
font:
  - file: "fonts/pixel.ttf"   # any small font; include Norwegian glyphs
    id: tiny
    size: 8
    glyphs: " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyzÆØÅæøå"

display:
  - platform: dot_matrix
    id: panel
    cs_pin: GPIO10
    num_modules: 8
    intensity: 3
    update_interval: 500ms
    lambda: |-
      // standard ESPHome font:
      it.printf(0, 0, id(tiny), "%.0f°", id(temp).state);
      // ...or the bundled flexible-width Norwegian font:
      it.print_dm(20, "Blåbær");
```

`it.print_dm(x, "...")` renders with the bundled flexible-width font (Æ Ø Å æ ø
å, plus the degree sign ° straight from UTF-8) and returns the next free column, so you can chain calls:
`int n = it.print_dm(0, "Kl "); it.print_dm(n, time_str);`. All the normal
`DisplayBuffer` methods (`print`, `printf`, `strftime`, `line`, `rectangle`,
`draw_pixel_at`, ...) work alongside it.

Centering: `it.print_dm_centered("...")` centers across the full display width
(the same behaviour as the action component's `write(..., centered: true)`).
For laying things out by hand, `it.measure_dm("...")` returns the pixel width a
string would occupy without drawing it, so you can position composites yourself:
`int x = (it.get_width() - it.measure_dm(s)) / 2; it.print_dm(x, s);`.

Live brightness from a lambda: `id(panel).intensity(8);`.

### Which one to use?

| | `dot_matrix:` component + actions | `display: - platform: dot_matrix` |
| --- | --- | --- |
| Model | push text, scroll with the bundled engine | redraw every interval |
| Best for | marquees, event-driven messages | dashboards, clock + sensors |
| Norwegian font | always (built in) | `it.print_dm()`, plus any ESPHome font |
| Scrolling | built-in `marquee` | via the writer lambda / display logic |

Both can run at once on different CS pins.

## Notes / TODO

- SPI clock is fixed at 1 MHz (`DATA_RATE_1MHZ`). MAX7219 supports up to
  10 MHz; expose as an option if you want faster refresh on long chains.
- `mirrored` and per-segment invert flags from the original driver are present
  in the class but not yet surfaced in the config schema.
- A `display`-platform variant (lambda/`it.print()` style) is possible later;
  this build keeps the original marquee/write API as actions.

## Layout

```
components/dot_matrix/
  __init__.py            # action component: config schema, SPI, actions
  display.py             # display platform registration
  dot_matrix.h           # action Component + SPIDevice + action classes
  dot_matrix.cpp         # action SPI transport + ported rendering core
  dot_matrix_display.h   # DisplayBuffer platform + print_dm()
  dot_matrix_display.cpp # platform transport, flush, custom font rendering
  dot_matrix_font.h      # font tables (verbatim from matrix.c)
```

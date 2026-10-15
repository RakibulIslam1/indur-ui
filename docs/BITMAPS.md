# Bitmap Studio and custom icons

IndurUI icons use one lit/unlit bit per pixel. Bitmap Studio resizes an ordinary image and exports a header that can be passed directly to the menu API.

Choose a [compatible MCU](COMPATIBILITY.md) before designing the interface. Converting to smaller icons saves bitmap storage; it does not make the full library compatible with a classic Nano/Uno. The converter runs on your computer.

## Desktop interface

Requires Python 3.10+, Pillow, and Tkinter. Windows Python installers normally include Tk. On Linux, your distribution may supply Tk through a separate `python3-tk` package. Pillow is installed with:

```sh
python -m pip install -r tools/requirements.txt
python tools/bitmap_converter.py --gui
```

1. Choose **Open image…**. PNG, JPEG, BMP, GIF (first frame), and WebP are accepted when supported by Pillow.
2. Pick 12, 16, 24, 32, 48, or 64 pixels, or type a custom `WIDTHxHEIGHT` from 1–128 per side.
3. Choose **contain**, **cover**, or **stretch**.
4. Adjust threshold, invert, padding, or dithering while looking at the preview. White means the OLED pixel lights up.
5. Enter a valid C++ icon name and choose **Save Arduino header…**.

The preview uses nearest-neighbor enlargement so you can see individual pixels. Start with simple high-contrast silhouettes; fine photographic detail rarely survives a 16×16 icon.

## Command line

```sh
python tools/bitmap_converter.py input.png \
  --output robotIcon.h --name robotIcon --size 32x32 \
  --fit contain --threshold 128 --padding 1 --preview preview.png
```

| Option | Default | Effect |
| --- | --- | --- |
| `input` | none | Source image; omit to open desktop interface |
| `-o`, `--output` | required for CLI | Generated `.h` path |
| `--name` | `robotIcon` | C++ symbol; letter first, then letters/digits/single underscores; keywords rejected |
| `--size` | `32x32` | `SIZE` for a square, or `WIDTHxHEIGHT`; each side 1–128 |
| `--fit contain` | contain | Preserve aspect ratio; pad unused space black |
| `--fit cover` | — | Preserve aspect ratio; crop to fill |
| `--fit stretch` | — | Fill exact dimensions, changing aspect ratio |
| `--threshold` | 128 | Grayscale cutoff 0–255; pixels at/above cutoff become white |
| `--invert` | off | Invert the composited source before fitting |
| `--dither` | off | Floyd–Steinberg conversion; replaces thresholding |
| `--padding` | 0 | Reserve an unlit border; must leave at least one pixel inside |
| `--preview` | none | Native-resolution PNG output |
| `--gui` | off | Open desktop interface |

Transparency composites onto black before inversion. EXIF orientation is applied before fitting. Output directories are created in CLI mode. Existing output files can be replaced, but the converter rejects using the input image path for either output or using one path for both header and preview. Keep originals separately if you experiment with overwriting outputs.

## Use the generated header

```cpp
#include "robotIcon.h"
indur::Id run = ui.addAction("Run", requestRun);
if (!ui.setIcon(run, robotIcon)) Serial.println(ui.errorText());
```

Or pass an icon at registration:

```cpp
ui.addAction("Run", requestRun, nullptr, indur::Root, robotIcon);
ui.addMenu("Tools", indur::Root, robotIcon);
```

`theme.iconSize` controls the menu slot. The renderer scales the source bitmap with nearest-neighbor sampling, preserves its aspect ratio, and centers it. Default slot: 32×32. Large source files do not create more physical screen detail. The slot also shrinks to leave room for labels and controls.

An empty `indur::Icon()` restores the numbered fallback tile. A list menu shows text rows; icons are used by the carousel/splash presentation. The bundled icon functions in `<IndurUIIcons.h>` return flash-backed 16×16 icons.

## Format and memory

Rows start on byte boundaries. The leftmost pixel is bit 7 of the first byte. Widths not divisible by 8 have zero padding bits at the end of each row.

```text
row bytes = (width + 7) / 8
total bytes = row bytes × height

16×16 = 32 bytes
24×24 = 72 bytes
32×32 = 128 bytes
64×64 = 512 bytes
```

For example, a nine-pixel row with only its leftmost and rightmost pixels lit is `0x80, 0x80`. Do not use vertically packed SSD1306 framebuffer bytes; those are a different representation.

The generated declaration is equivalent to:

```cpp
static const uint8_t robotIcon_data[] INDURUI_PROGMEM = { /* packed pixels */ };
static const indur::Icon robotIcon(robotIcon_data, 32, 32, indur::Memory::Flash);
```

On AVR, `Memory::Flash` uses program-memory reads; ESP32 uses memory-mapped const data. For a mutable runtime bitmap, allocate enough RAM and pass `Memory::Ram`. Pointer lifetime and buffer length are the application's responsibility; dimensions do not allocate or copy the bitmap. The core accepts dimensions up to 256×256, while the converter deliberately caps exports at 128×128.

Only include a generated static bitmap header where needed. Including it in several translation units may duplicate its data. Large AVR flash layouts may require a board-specific adapter because ordinary `pgm_read_byte` uses the platform's standard program-memory addressing model.

## Reproduce included assets

```sh
python tools/generate_icons.py
```

This script generates the eight built-in geometric icons, the example header, and `docs/assets/robot-icon.png`. The original icon artwork is included under this repository's MIT license. Convert third-party artwork only when you have permission to use it.

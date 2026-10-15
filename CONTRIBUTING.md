# Contributing

Keep the interface useful across projects: application-owned variables, explicit callbacks, bounded layout, and cooperative input/rendering. Avoid adding robot pin maps or motion algorithms to the core. Changes to storage require compatibility review and failure-path tests.

## Native core tests

On a host with GCC or Clang:

```sh
mkdir -p .build
g++ -std=c++11 -Wall -Wextra -pedantic -Isrc -Itests/fakes \
  tests/test_core.cpp src/IndurUI.cpp -o .build/test_core
.build/test_core
```

The fake Arduino header supplies GPIO inputs only; the native tests exercise the real library core and button adapter. Test canvases reject off-screen drawing coordinates. Memory storage injects failures into byte reads/writes and flushes. Tests cover interruption recovery, canceled edits, bounds, unchanged saves, application-region isolation, nested navigation, custom pages, confirmation, button rollover, and EEPROM API variants.

On Windows, use a Visual Studio developer PowerShell/command prompt:

```powershell
New-Item -ItemType Directory -Force .build | Out-Null
cl /nologo /EHsc /std:c++14 /D_CRT_SECURE_NO_WARNINGS /Isrc /Itests/fakes tests/test_core.cpp src/IndurUI.cpp /Fe:.build/test_core.exe /Fo:.build/
.build/test_core.exe
```

## Converter tests

```sh
python -m pip install -r tools/requirements.txt
python -m unittest discover -s tests -p 'test_converter.py' -v
python tests/check_gui.py
```

The GUI check requires Tk and a graphical session; it hides its window, opens a temporary image, and invokes the real preview/export flow. On headless Linux use `xvfb-run -a python tests/check_gui.py` with Tk/Xvfb installed.

## Firmware example checks

```sh
python -m pip install platformio==6.2.0
pio run -e esp32s3-quick -e esp32s3-two -e esp32s3-persistent -e esp32s3-dashboard -e esp32s3-icons
pio run -e mega-quick -e mega-two -e mega-persistent -e mega-dashboard -e mega-icons
pio run -e mini_ui_test -e mini_ui_test_2buttons
python tests/check_boards.py
```

The pinned test environments use Espressif32 6.13.0 and Atmel AVR 5.3.0, with Adafruit SSD1306/GFX/BusIO dependencies. Successful compilation establishes API/build compatibility, not physical validation. Flash/static-RAM reports exclude runtime allocations such as UI metadata and the SSD1306 framebuffer. Record hardware testing separately, including board, screen size, address, wiring, timing, and EEPROM implementation.

Local checks use MSVC 14.50, Python 3.11, Pillow 12.2.0, Tk 8.6, and PlatformIO 6.2.0. CI additionally runs the core with GCC and address/undefined-behavior sanitizers on Linux. `check_boards.py` uses PlatformIO's installed AVR compiler and real device headers to accept ATmega2560, reject ATmega328P/168/32U4 with the expected memory explanation, and reject an unsupported Arduino architecture. It checks target policy, not a complete memory budget.

## Style and assets

Use C++11 in the library and examples. Keep useful comments, descriptive names, and conventional formatting. `.clang-format` provides the C++ style. Python tooling requires 3.10+.

Rebuild geometric icons:

```sh
python tools/generate_icons.py
```

Apply clang-format to the generated C++ headers after generation. To rebuild screen previews from the core's actual drawing calls:

```sh
g++ -std=c++11 -Isrc tests/render_screens.cpp src/IndurUI.cpp -o .build/render_screens
.build/render_screens
```

Those older SVG previews retain layout coordinates but use substitute text glyphs. The website's **exact framebuffer captures** instead use the real GFX text and drawing renderer:

```sh
python tools/render_oled.py --gfx-dir ".pio/libdeps/esp32s3-quick/Adafruit GFX Library"
```

This requires Adafruit GFX 1.12.6 and a host C++ compiler. Linux CI checks out the official dependency at its `1.12.6` tag. Host shims supply Arduino/Print interfaces only; GFX itself is unmodified. The generated JSON and PNG assets in `website/frames/` must match a fresh render. GFX's own license remains in the external dependency. See the [upstream project](https://github.com/adafruit/Adafruit-GFX-Library/tree/1.12.6).

## Packaging

Arduino packaging follows the [Arduino library specification](https://docs.arduino.cc/arduino-cli/library-specification). Keep version strings in `library.properties` and `library.json` aligned with the release tag. The release ZIP contains one `IndurUI/` folder with the library and examples; never package `.pio`, private robot sources, credentials, or build logs.

After committing and tagging a release (the explicit newline settings also make the archive reproducible on Windows):

```sh
mkdir -p dist
git -c core.autocrlf=false -c core.eol=lf archive --format=zip --prefix=IndurUI/ --output=dist/IndurUI-1.1.0.zip v1.1.0
```

Run `arduino-lint --compliance specification --library-manager submit` against a clean archive extracted into a folder named `IndurUI`. Build caches and local tool executables are excluded by Git; linting a developer checkout with those untracked folders gives irrelevant errors. Arduino Library Manager registration follows the [official registry procedure](https://github.com/arduino/library-registry#adding-a-library-to-library-manager), independently of creating a GitHub release.

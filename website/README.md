# IndurUI Bitmap Studio website

Live site: **https://rakibulislam1.github.io/indur-ui/**

A static browser application. No server-side processing, account, analytics, remote fonts, or image upload service is used. User images are decoded and converted locally; the generated header and preview PNG are downloaded from in-memory blobs.

## Use it

1. Open the site and choose an image, drag one into the workbench, or try a sample.
2. Choose a square preset or custom dimensions from 1–128 pixels per side.
3. Adjust contain/cover/stretch, padding, threshold, inversion, or Floyd–Steinberg dithering.
4. Inspect the bitmap, enable the pixel grid, or preview it over a captured menu background. Retouch pixels with click/drag, mirror or rotate, and undo up to 20 edits. Conversion controls rebuild the image and clear manual edits.
5. Enter a valid C++ icon name and download the `.h` file. PNG preview and clipboard export are also available.

PNG, JPEG, SVG, WebP, GIF first frames, and BMP can be decoded when supported by the browser. Files are limited to 20 MB and decoded images to 16 million pixels. Invalid input keeps the previous image. Invalid conversion settings pause export, and invalid C++ names disable header export while still permitting PNG downloads.

The output is the same IndurUI format as the desktop converter: MSB-first bits with each horizontal row padded to a byte boundary. The source array uses `INDURUI_PROGMEM`, and the exported `Icon` uses `Memory::Flash`. Browser resampling and dithering can produce different artistic results from Pillow; the byte representation and library API are the same. The menu preview composites the user's icon into a real captured background; it does not execute firmware.

## OLED Lab

Eight gallery captures are generated with the real IndurUI core, its `GfxCanvas` adapter, and unmodified Adafruit GFX 1.12.6. Values are fixed demo data. Each has a native 128×64 PNG, a nearest-neighbor 4× PNG, and its raw 1024-byte SSD1306 page buffer as JSON in `frames/`. These are exact rendered pixels, not photographs or replacement-font illustrations.

Select a gallery screen or import JSON/a complete `INDURUI_FRAME {...}` serial line. Export at 1×, 4×, or 8× without smoothing, or copy raw JSON. The parser validates dimensions, format, hexadecimal data, and byte count before replacing the current frame.

With [MiniRobotTest](../docs/MINI_ROBOT_TEST.md), desktop Chrome/Edge can connect to the board through Web Serial at 115200 baud. The browser's port picker requires a user action. **Capture OLED** sends `p\n` and reads the firmware's buffer response; no firmware is installed by the website. Close other serial monitors first. Unsupported browsers can still import files or pasted captures. Data stays local. USB navigation and physical hardware have not been validated by automated browser tests; the tests simulate the port and verify the command/response protocol.

Classic Nano/Uno are unsupported by the full library. The website runs on your computer or phone and does not run on the MCU. See [MCU compatibility](https://github.com/RakibulIslam1/indur-ui/blob/main/docs/COMPATIBILITY.md).

## Local preview

From the repository root:

```sh
python -m http.server 8080 --bind 127.0.0.1 --directory website
```

Open `http://127.0.0.1:8080/`. Use an HTTP server; directly opening `index.html` as a file can block ES module imports. Current browsers with Canvas 2D and ES modules are required. Clipboard export requires a secure context (HTTPS or localhost); a manual-copy fallback selects the header when clipboard access is blocked.

## Checks

Requires Node.js 20+ and a browser. From the repository root:

```sh
node --test tests/test_bitmap.mjs tests/test_oled.mjs
npm install --prefix .build/web-tools --no-save playwright@1.64.1
.build/web-tools/node_modules/.bin/playwright install --with-deps chromium
node tests/test_website.cjs
```

On Windows, the browser check uses installed Microsoft Edge by default. `BROWSER_EXECUTABLE` can select another Chromium executable. `PLAYWRIGHT_PACKAGE` can select an alternate Playwright installation. `WEBSITE_URL` can target the published website instead of starting a temporary local server. Test screenshots and downloads go to ignored `.build/website/`.

Checks cover bit order, non-byte widths, fitting, transparency, threshold/dither/padding, names, pixel edits/transforms/undo, desktop/mobile layout, file upload and drop, exact exported bitmap values, every decoded OLED pixel, PNG dimensions, clipboard, invalid inputs, JSON imports, a simulated Web Serial session, and unexpected external page requests.

## Deployment

`.github/workflows/pages.yml` validates the converter and browser behavior, then uploads **only `website/`** for GitHub Pages. It publishes from `main`; pull requests run validation without deploying. GitHub Pages must use GitHub Actions as its source. No Arduino firmware or private project files are deployed to the site.

MIT license; part of the IndurUI project by Rakibul Islam.

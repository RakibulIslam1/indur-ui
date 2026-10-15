# IndurUI Bitmap Studio website

Live site: **https://rakibulislam1.github.io/indur-ui/**

A static browser application. No server-side processing, account, analytics, remote fonts, or image upload service is used. User images are decoded and converted locally; the generated header and preview PNG are downloaded from in-memory blobs.

## Use it

1. Open the site and choose an image, drag one into the workbench, or try a sample.
2. Choose a square preset or custom dimensions from 1–128 pixels per side.
3. Adjust contain/cover/stretch, padding, threshold, inversion, or Floyd–Steinberg dithering.
4. Inspect the bitmap, enable the pixel grid, or see an illustrative OLED menu preview.
5. Enter a valid C++ icon name and download the `.h` file. PNG preview and clipboard export are also available.

PNG, JPEG, SVG, WebP, GIF first frames, and BMP can be decoded when supported by the browser. Files are limited to 20 MB and decoded images to 16 million pixels. Invalid input keeps the previous image. Invalid conversion settings pause export, and invalid C++ names disable header export while still permitting PNG downloads.

The output is the same IndurUI format as the desktop converter: MSB-first bits with each horizontal row padded to a byte boundary. The source array uses `INDURUI_PROGMEM`, and the exported `Icon` uses `Memory::Flash`. Browser resampling and dithering can produce different artistic results from Pillow; the byte representation and library API are the same. The OLED menu preview is illustrative, not a firmware simulator.

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
node --test tests/test_bitmap.mjs
npm install --prefix .build/web-tools --no-save playwright@1.64.1
.build/web-tools/node_modules/.bin/playwright install --with-deps chromium
node tests/test_website.cjs
```

On Windows, the browser check uses installed Microsoft Edge by default. `BROWSER_EXECUTABLE` can select another Chromium executable. `PLAYWRIGHT_PACKAGE` can select an alternate Playwright installation. `WEBSITE_URL` can target the published website instead of starting a temporary local server. Test screenshots and downloads go to ignored `.build/website/`.

Checks cover bit order, non-byte widths, fitting, transparency, threshold/dither/padding, names, desktop/mobile layout, file upload and drop, exact exported bitmap values, PNG dimensions, clipboard, invalid images, and unexpected external page requests.

## Deployment

`.github/workflows/pages.yml` validates the converter and browser behavior, then uploads **only `website/`** for GitHub Pages. It publishes from `main`; pull requests run validation without deploying. GitHub Pages must use GitHub Actions as its source. No Arduino firmware or private project files are deployed to the site.

MIT license; part of the IndurUI project by Rakibul Islam.

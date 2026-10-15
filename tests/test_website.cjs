const assert = require("node:assert/strict");
const http = require("node:http");
const fs = require("node:fs/promises");
const path = require("node:path");
const { chromium } = require(
  process.env.PLAYWRIGHT_PACKAGE ||
    path.resolve(__dirname, "../.build/web-tools/node_modules/playwright"),
);
const root = path.resolve(__dirname, "..");
const website = path.join(root, "website");
const artifacts = path.join(root, ".build/website");
const types = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css",
  ".mjs": "text/javascript",
  ".svg": "image/svg+xml",
};

async function main() {
  await fs.mkdir(artifacts, { recursive: true });
  const external = process.env.WEBSITE_URL;
  const server = external
    ? null
    : http.createServer(async (request, response) => {
        try {
          const pathname = decodeURIComponent(
            new URL(request.url, "http://localhost").pathname,
          );
          const filename = path.resolve(
            website,
            "." + (pathname.endsWith("/") ? pathname + "index.html" : pathname),
          );
          if (!filename.startsWith(website + path.sep))
            throw new Error("Invalid path");
          const content = await fs.readFile(filename);
          response.writeHead(200, {
            "Content-Type":
              types[path.extname(filename)] || "application/octet-stream",
          });
          response.end(content);
        } catch {
          response.writeHead(404);
          response.end("Not found");
        }
      });
  if (server)
    await new Promise((resolve) => server.listen(0, "127.0.0.1", resolve));
  const url = external || `http://127.0.0.1:${server.address().port}/`;
  const executablePath =
    process.env.BROWSER_EXECUTABLE ||
    (process.platform === "win32"
      ? "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe"
      : undefined);
  let browser;
  try {
    browser = await chromium.launch({ headless: true, executablePath });
    const context = await browser.newContext({
      viewport: { width: 1440, height: 1100 },
      acceptDownloads: true,
      permissions: ["clipboard-read", "clipboard-write"],
    });
    const page = await context.newPage();
    const errors = [];
    const requests = [];
    page.on("pageerror", (error) => errors.push(error.message));
    page.on("request", (request) => requests.push(request.url()));
    await page.goto(url, { waitUntil: "networkidle" });
    await page.waitForFunction(() =>
      document
        .querySelector("#header-code")
        .textContent.includes("INDURUI_PROGMEM"),
    );
    assert.equal(await page.locator("#byte-count").innerText(), "128 B");
    await page.screenshot({
      path: path.join(artifacts, "desktop.png"),
      fullPage: true,
    });
    await page
      .locator("#studio")
      .screenshot({ path: path.join(artifacts, "studio.png") });
    await page.locator("#menu-tab").click();
    assert.equal(await page.locator("#menu-surface").isVisible(), true);
    await page.locator("#bitmap-tab").click();
    await page.locator("#show-grid").check();
    assert.equal(await page.locator("#pixel-grid").isVisible(), true);

    const image = await page.evaluate(() => {
      const c = document.createElement("canvas");
      c.width = 9;
      c.height = 2;
      const x = c.getContext("2d");
      x.fillStyle = "black";
      x.fillRect(0, 0, 9, 2);
      x.fillStyle = "white";
      [
        [0, 0],
        [8, 0],
        [1, 1],
      ].forEach(([a, b]) => x.fillRect(a, b, 1, 1));
      return c.toDataURL("image/png").split(",")[1];
    });
    await page
      .locator("#image-file")
      .setInputFiles({
        name: "pattern.png",
        mimeType: "image/png",
        buffer: Buffer.from(image, "base64"),
      });
    await page.waitForFunction(() =>
      document
        .querySelector("#upload-help")
        .textContent.includes("pattern.png"),
    );
    await page.locator("#size-preset").selectOption("custom");
    await page.locator("#bitmap-width").fill("9");
    await page.locator("#bitmap-height").fill("2");
    await page.locator("#fit").selectOption("stretch");
    await page.locator("#icon-name").fill("patternIcon");
    const code = await page.locator("#header-code").innerText();
    assert.ok(code.includes("0x80, 0x80, 0x40, 0x00,"), code);
    assert.ok(code.includes("patternIcon_data, 9, 2"));
    const headerDownload = page.waitForEvent("download");
    await page.locator("#download-header").click();
    const exported = await headerDownload;
    assert.equal(exported.suggestedFilename(), "patternIcon.h");
    assert.equal(
      (await fs.readFile(await exported.path(), "utf8")).replace(/\r\n/g, "\n"),
      code.replace(/\r\n/g, "\n"),
    );
    await exported.saveAs(path.join(artifacts, "patternIcon.h"));
    await page.locator("#copy-header").click();
    assert.equal(
      (await page.evaluate(() => navigator.clipboard.readText())).replace(
        /\r\n/g,
        "\n",
      ),
      code.replace(/\r\n/g, "\n"),
    );
    const pngDownload = page.waitForEvent("download");
    await page.locator("#download-png").click();
    const png = await pngDownload;
    const pngData = await fs.readFile(await png.path());
    assert.equal(pngData.readUInt32BE(16), 9);
    assert.equal(pngData.readUInt32BE(20), 2);
    await page.locator("#icon-name").fill("class");
    assert.equal(await page.locator("#download-header").isDisabled(), true);
    assert.equal(await page.locator("#download-png").isDisabled(), false);
    await page.locator("#icon-name").fill("patternIcon");
    await page.locator("#bitmap-width").fill("0");
    assert.equal(await page.locator("#download-png").isDisabled(), true);
    await page.locator("#bitmap-width").fill("9");
    assert.equal(await page.locator("#download-header").isEnabled(), true);
    await page
      .locator("#image-file")
      .setInputFiles({
        name: "wrong.txt",
        mimeType: "text/plain",
        buffer: Buffer.from("not an image"),
      });
    assert.ok(
      (await page.locator("#feedback").innerText()).includes("Choose a PNG"),
    );
    await page
      .locator("#image-file")
      .setInputFiles({
        name: "broken.png",
        mimeType: "image/png",
        buffer: Buffer.from("broken image"),
      });
    await page.waitForFunction(() =>
      document
        .querySelector("#feedback")
        .textContent.includes("could not be read"),
    );
    await page.locator("#reset").click();
    assert.equal(await page.locator("#icon-name").inputValue(), "robotIcon");
    assert.equal(await page.locator("#byte-count").innerText(), "128 B");
    await page.locator("#dither").check();
    assert.equal(await page.locator("#threshold").isDisabled(), true);
    await page.locator("#dither").uncheck();
    await page.locator("#invert").check();
    assert.ok(
      (await page.locator("#header-code").innerText()).includes(
        "INDURUI_PROGMEM",
      ),
    );
    await page.locator("#reset").click();
    const uploaded = await page.evaluate(() => {
      const dt = new DataTransfer();
      dt.items.add(
        new File(
          [
            '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><rect width="16" height="16" fill="white"/></svg>',
          ],
          "square.svg",
          { type: "image/svg+xml" },
        ),
      );
      document
        .querySelector("#dropzone")
        .dispatchEvent(
          new DragEvent("drop", {
            bubbles: true,
            cancelable: true,
            dataTransfer: dt,
          }),
        );
      return true;
    });
    assert.ok(uploaded);
    await page.waitForFunction(() =>
      document.querySelector("#upload-help").textContent.includes("square.svg"),
    );
    assert.equal(await page.locator("#lit-count").innerText(), "100%");
    await page.locator("#reset").click();
    await page.setViewportSize({ width: 390, height: 844 });
    await page.evaluate(() => window.scrollTo(0, 0));
    await page.screenshot({
      path: path.join(artifacts, "mobile.png"),
      fullPage: true,
    });
    assert.equal(
      await page.evaluate(
        () => document.documentElement.scrollWidth <= innerWidth,
      ),
      true,
      "Mobile page overflows horizontally",
    );
    await page.locator("#menu-tab").click();
    assert.equal(await page.locator("#menu-surface").isVisible(), true);
    await page.locator("#download-header").click();
    assert.deepEqual(errors, []);
    const origin = new URL(url).origin;
    assert.deepEqual(
      requests.filter(
        (request) =>
          new URL(request).origin !== origin &&
          !request.startsWith("blob:") &&
          !request.startsWith("data:"),
      ),
      [],
      "The converter made an external request",
    );
    console.log(
      "Browser checks passed: desktop/mobile, upload/drop, preview, exact header bytes, PNG, clipboard, errors, and local-only image processing",
    );
  } finally {
    if (browser) await browser.close();
    if (server) await new Promise((resolve) => server.close(resolve));
  }
}
main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});

const assert = require("node:assert/strict");
const fs = require("node:fs/promises");
const path = require("node:path");

exports.installSerialMock = async function (page) {
  await page.addInitScript(() => {
    let input;
    const port = {
      readable: null,
      writable: null,
      async open(options) {
        window.__serialBaud = options.baudRate;
        this.readable = new ReadableStream({
          start(controller) {
            input = controller;
          },
        });
        this.writable = new WritableStream({
          write(bytes) {
            const command = new TextDecoder().decode(bytes);
            window.__serialWrites = (window.__serialWrites || "") + command;
            if (command === "p\n") {
              const encoded = new TextEncoder().encode(
                "[UI] log\r\nINDURUI_FRAME " + window.__serialFrame + "\r\n",
              );
              input.enqueue(encoded.slice(0, 100));
              input.enqueue(encoded.slice(100));
            }
          },
        });
      },
      async close() {
        window.__serialClosed = true;
        this.readable = this.writable = null;
      },
    };
    Object.defineProperty(navigator, "serial", {
      configurable: true,
      value: {
        async requestPort() {
          return port;
        },
        addEventListener() {},
      },
    });
  });
};

exports.checkOledLab = async function (page, artifacts) {
  const baseline = await page.locator("#header-code").innerText();
  await page.locator("#edit-pixels").check();
  await page.locator("#bitmap-canvas").click({ position: { x: 3, y: 3 } });
  assert.notEqual(await page.locator("#header-code").innerText(), baseline);
  await page.locator("#undo-pixels").click();
  assert.equal(await page.locator("#header-code").innerText(), baseline);
  await page.locator("#flip-pixels").click();
  await page.locator("#undo-pixels").click();
  assert.equal(await page.locator("#header-code").innerText(), baseline);
  await page.locator("#rotate-pixels").click();
  await page.locator("#undo-pixels").click();
  assert.equal(await page.locator("#header-code").innerText(), baseline);
  await page.locator("#edit-pixels").uncheck();

  await page.locator('[data-screen="editor"]').click();
  await page.waitForFunction(
    () =>
      document.querySelector("#capture-title").textContent === "Numeric editor",
  );
  assert.equal(
    await page.locator('[data-screen="editor"]').getAttribute("aria-pressed"),
    "true",
  );
  await page.locator("#capture-copy").click();
  const source = JSON.parse(
    await page.evaluate(() => navigator.clipboard.readText()),
  );
  assert.equal(source.width, 128);
  assert.equal(source.height, 64);
  assert.equal(source.data.length, 2048);
  const pixels = await page
    .locator("#capture-canvas")
    .evaluate((canvas) =>
      Array.from(canvas.getContext("2d").getImageData(0, 0, 128, 64).data),
    );
  const bytes = Buffer.from(source.data, "hex");
  for (let y = 0; y < 64; ++y)
    for (let x = 0; x < 128; ++x) {
      const pixel = (y * 128 + x) * 4;
      const expected =
        ((bytes[x + Math.floor(y / 8) * 128] >> (y & 7)) & 1) * 255;
      assert.deepEqual(pixels.slice(pixel, pixel + 4), [
        expected,
        expected,
        expected,
        255,
      ]);
    }
  const downloadTask = page.waitForEvent("download");
  await page.locator("#capture-download").click();
  const download = await downloadTask;
  const png = await fs.readFile(await download.path());
  assert.equal(png.readUInt32BE(16), 512);
  assert.equal(png.readUInt32BE(20), 256);
  await download.saveAs(path.join(artifacts, "oled-editor-4x.png"));
  await page.locator("#capture-input").fill("broken JSON");
  await page.locator("#capture-import").click();
  assert.ok(
    (await page.locator("#capture-status").innerText()).includes(
      "complete INDURUI_FRAME",
    ),
  );
  assert.equal(
    await page.locator("#capture-title").innerText(),
    "Numeric editor",
  );
  await page
    .locator("#capture-input")
    .fill("INDURUI_FRAME " + JSON.stringify(source));
  await page.locator("#capture-import").click();
  assert.equal(
    await page.locator("#capture-title").innerText(),
    "Your robot’s OLED",
  );
  await page
    .locator("#capture-file")
    .setInputFiles({
      name: "robot.json",
      mimeType: "application/json",
      buffer: Buffer.from(JSON.stringify(source)),
    });
  await page.waitForFunction(
    () => document.querySelector("#capture-title").textContent === "robot.json",
  );

  await page.evaluate((frame) => {
    window.__serialFrame = JSON.stringify(frame);
  }, source);
  await page.locator("#serial-connect").click();
  assert.equal(await page.evaluate(() => window.__serialBaud), 115200);
  await page.locator("#serial-capture").click();
  await page.waitForFunction(
    () =>
      document.querySelector("#capture-title").textContent ===
      "Captured from your robot",
  );
  assert.equal(await page.evaluate(() => window.__serialWrites), "p\n");
  await page.locator("#serial-connect").click();
  assert.equal(await page.evaluate(() => window.__serialClosed), true);
  assert.equal(await page.locator("#serial-capture").isDisabled(), true);
  await page
    .locator("#oled-lab")
    .screenshot({ path: path.join(artifacts, "oled-lab.png") });
};

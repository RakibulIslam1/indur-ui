import test from "node:test";
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { parseFrame } from "../website/oled.mjs";
import { transformBits } from "../website/bitmap.mjs";

test("SSD1306 page bytes decode to the exact pixel positions", () => {
  const frame = parseFrame({
    format: "indurui-ssd1306-v1",
    width: 2,
    height: 16,
    data: "01800240",
  });
  assert.deepEqual(
    [...frame.bits.entries()].filter(([, bit]) => bit).map(([index]) => index),
    [0, 15, 18, 29],
  );
  assert.equal(frame.bytes.length, 4);
  const serial =
    "Boot log\r\nINDURUI_FRAME " +
    JSON.stringify(frame.source) +
    "\r\n[UI] log\n";
  assert.deepEqual(parseFrame(serial).bits, frame.bits);
});

test("truncated, oversized, malformed, or wrong-format captures are rejected", () => {
  const valid = {
    format: "indurui-ssd1306-v1",
    width: 2,
    height: 8,
    data: "abcd",
  };
  for (const invalid of [
    null,
    {},
    "not JSON",
    { ...valid, height: 9 },
    { ...valid, width: 257 },
    { ...valid, width: 1.5 },
    { ...valid, data: "zzzz" },
    { ...valid, data: "abc" },
    { ...valid, format: "other" },
    "x".repeat(65537),
  ])
    assert.throws(() => parseFrame(invalid));
  assert.equal(parseFrame({ ...valid, data: "ABCD" }).source.data, "abcd");
});

test("all eight published captures have complete, distinct nonempty framebuffers", async () => {
  const values = new Set();
  for (const name of [
    "dashboard",
    "menu",
    "settings",
    "editor",
    "confirmation",
    "progress",
    "notification",
    "splash",
  ]) {
    const frame = parseFrame(
      await readFile(
        new URL(`../website/frames/${name}.json`, import.meta.url),
        "utf8",
      ),
    );
    assert.equal(frame.width, 128);
    assert.equal(frame.height, 64);
    assert.equal(frame.bytes.length, 1024);
    const lit = frame.bits.reduce((sum, bit) => sum + bit, 0);
    assert.ok(lit > 50 && lit < 8192, name);
    values.add(frame.source.data);
  }
  assert.equal(values.size, 8);
});

test("pixel transforms preserve non-square images and undo-friendly data", () => {
  const bits = new Uint8Array([1, 0, 0, 0, 1, 1]);
  assert.deepEqual(
    [...transformBits(bits, 3, 2, "flip").bits],
    [0, 0, 1, 1, 1, 0],
  );
  let rotated = transformBits(bits, 3, 2, "rotate");
  assert.equal(rotated.width, 2);
  assert.equal(rotated.height, 3);
  assert.deepEqual([...rotated.bits], [0, 1, 1, 0, 1, 0]);
  for (let i = 0; i < 3; ++i)
    rotated = transformBits(
      rotated.bits,
      rotated.width,
      rotated.height,
      "rotate",
    );
  assert.deepEqual(rotated.bits, bits);
  assert.deepEqual([...bits], [1, 0, 0, 0, 1, 1]);
  assert.throws(() => transformBits(bits, 2, 2, "rotate"));
  assert.throws(() => transformBits(bits, 3, 2, "unknown"));
});

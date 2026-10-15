import test from "node:test";
import assert from "node:assert/strict";
import {
  dimensions,
  fitRect,
  monochrome,
  pack,
  header,
  validateName,
} from "../website/bitmap.mjs";

function rgba(width, height, value = 0, alpha = 255) {
  const result = new Uint8ClampedArray(width * height * 4);
  for (let i = 0; i < width * height; i++)
    result.set([value, value, value, alpha], i * 4);
  return result;
}

test("row padding and MSB-first bytes match the Arduino format", () => {
  const bits = new Uint8Array(18);
  for (const index of [0, 8, 10]) bits[index] = 1;
  assert.deepEqual([...pack(bits, 9, 2)], [0x80, 0x80, 0x40, 0x00]);
  assert.deepEqual([...pack(new Uint8Array(9).fill(1), 9, 1)], [0xff, 0x80]);
});

test("fitting preserves proportions, crops, stretches and reserves padding", () => {
  assert.deepEqual(fitRect(20, 10, 32, 32, 2), {
    x: 2,
    y: 9,
    width: 28,
    height: 14,
  });
  assert.deepEqual(fitRect(20, 10, 32, 32, 2, "cover"), {
    x: -12,
    y: 2,
    width: 56,
    height: 28,
  });
  assert.deepEqual(fitRect(20, 10, 32, 32, 2, "stretch"), {
    x: 2,
    y: 2,
    width: 28,
    height: 28,
  });
  assert.throws(() => dimensions(0, 32));
  assert.throws(() => dimensions(3.5, 32));
  assert.throws(() => dimensions(32, 129));
  assert.throws(() => fitRect(20, 10, 32, 32, 16));
  assert.throws(() => fitRect(20, 10, 32, 32, 0, "bad"));
});

test("alpha composites on black and inversion preserves the letterbox", () => {
  const transparent = rgba(4, 4, 255, 0);
  assert.equal(
    monochrome(transparent, { width: 4, height: 4 }).reduce((a, b) => a + b),
    0,
  );
  assert.equal(
    monochrome(transparent, { width: 4, height: 4, invert: true }).reduce(
      (a, b) => a + b,
    ),
    16,
  );
  const inverted = monochrome(rgba(4, 4), {
    width: 4,
    height: 4,
    invert: true,
    contentRect: { x: 1, y: 1, width: 2, height: 2 },
  });
  assert.equal(
    inverted.reduce((a, b) => a + b),
    4,
  );
});

test("threshold and dithering produce meaningful binary output", () => {
  const gray = rgba(16, 16, 128);
  assert.equal(
    monochrome(gray, { width: 16, height: 16 }).reduce((a, b) => a + b),
    256,
  );
  const dithered = monochrome(gray, {
    width: 16,
    height: 16,
    dither: true,
    threshold: 255,
  });
  const lit = dithered.reduce((a, b) => a + b);
  assert.ok(lit >= 120 && lit <= 136);
  const padded = monochrome(rgba(4, 4), {
    width: 4,
    height: 4,
    threshold: 0,
    padding: 1,
  });
  assert.deepEqual(
    [...padded],
    [0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0],
  );
  assert.throws(() =>
    monochrome(gray, { width: 16, height: 16, threshold: -1 }),
  );
});

test("headers are library-ready and invalid symbols cannot become code", () => {
  const source = header(
    new Uint8Array([0x80, 0x80, 0x40, 0x00]),
    9,
    2,
    "myIcon",
  );
  assert.ok(source.includes("0x80, 0x80, 0x40, 0x00,"));
  assert.ok(source.includes("myIcon_data[] INDURUI_PROGMEM"));
  assert.ok(source.includes("myIcon(myIcon_data, 9, 2, indur::Memory::Flash)"));
  for (const name of [
    "class",
    "8bit",
    "bad-name",
    "a__b",
    "_reserved",
    "<script>",
    "co_await",
  ])
    assert.throws(() => validateName(name));
  assert.equal(validateName("roverIcon_2"), "roverIcon_2");
  assert.throws(() => header(new Uint8Array(1), 9, 2, "icon"));
});

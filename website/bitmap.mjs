// Hardware format shared by browser previews and Arduino header export.
const keywords = new Set(
  (
    "alignas alignof and and_eq asm atomic_cancel atomic_commit atomic_noexcept auto " +
    "bitand bitor bool break case catch char char8_t char16_t char32_t class compl concept const consteval " +
    "constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double " +
    "dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable " +
    "namespace new noexcept not not_eq nullptr operator or or_eq private protected public register " +
    "reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch " +
    "synchronized template this thread_local throw true try typedef typeid typename union unsigned using " +
    "virtual void volatile wchar_t while xor xor_eq"
  ).split(/\s+/),
);

export function dimensions(width, height) {
  if (
    ![width, height].every(
      (value) => Number.isInteger(value) && value >= 1 && value <= 128,
    )
  ) {
    throw new Error(
      "Width and height must be whole numbers between 1 and 128 pixels.",
    );
  }
}

export function validateName(name) {
  if (!/^[A-Za-z][A-Za-z0-9_]*$/.test(name) || name.includes("__")) {
    throw new Error(
      "Use a name starting with a letter, then letters, digits, or single underscores.",
    );
  }
  if (keywords.has(name))
    throw new Error("Choose an icon name that is not a C++ keyword.");
  if (name.length > 48)
    throw new Error("Keep the icon name to 48 characters or fewer.");
  return name;
}

export function fitRect(
  sourceWidth,
  sourceHeight,
  width,
  height,
  padding = 0,
  fit = "contain",
) {
  dimensions(width, height);
  if (
    !Number.isFinite(sourceWidth) ||
    !Number.isFinite(sourceHeight) ||
    sourceWidth <= 0 ||
    sourceHeight <= 0
  ) {
    throw new Error("This image has no readable pixels.");
  }
  if (
    !Number.isInteger(padding) ||
    padding < 0 ||
    padding * 2 >= Math.min(width, height)
  ) {
    throw new Error("Padding must leave at least one image pixel inside.");
  }
  if (!["contain", "cover", "stretch"].includes(fit))
    throw new Error("Choose a valid fitting mode.");
  const innerWidth = width - 2 * padding;
  const innerHeight = height - 2 * padding;
  if (fit === "stretch")
    return { x: padding, y: padding, width: innerWidth, height: innerHeight };
  const scale = (fit === "cover" ? Math.max : Math.min)(
    innerWidth / sourceWidth,
    innerHeight / sourceHeight,
  );
  const fittedWidth = Math.max(1, Math.round(sourceWidth * scale));
  const fittedHeight = Math.max(1, Math.round(sourceHeight * scale));
  return {
    x: padding + Math.floor((innerWidth - fittedWidth) / 2),
    y: padding + Math.floor((innerHeight - fittedHeight) / 2),
    width: fittedWidth,
    height: fittedHeight,
  };
}

export function monochrome(
  rgba,
  {
    width,
    height,
    threshold = 128,
    invert = false,
    dither = false,
    padding = 0,
    contentRect = null,
  },
) {
  dimensions(width, height);
  if (rgba.length !== width * height * 4)
    throw new Error("Image pixels do not match the chosen dimensions.");
  if (!Number.isInteger(threshold) || threshold < 0 || threshold > 255)
    throw new Error("Threshold must be between 0 and 255.");
  fitRect(width, height, width, height, padding);
  const rect = contentRect || { x: 0, y: 0, width, height };
  const gray = new Float64Array(width * height);
  const bits = new Uint8Array(width * height);
  for (let i = 0; i < gray.length; i++) {
    const offset = i * 4;
    const luminance = Math.round(
      ((299 * rgba[offset] + 587 * rgba[offset + 1] + 114 * rgba[offset + 2]) *
        rgba[offset + 3]) /
        255000,
    );
    const x = i % width;
    const y = Math.floor(i / width);
    const inside =
      x >= rect.x &&
      x < rect.x + rect.width &&
      y >= rect.y &&
      y < rect.y + rect.height;
    gray[i] = invert && inside ? 255 - luminance : luminance;
  }
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = y * width + x;
      const lit = gray[i] >= (dither ? 128 : threshold);
      bits[i] = lit ? 1 : 0;
      if (dither) {
        const error = gray[i] - (lit ? 255 : 0);
        if (x + 1 < width) gray[i + 1] += (error * 7) / 16;
        if (y + 1 < height) {
          if (x > 0) gray[i + width - 1] += (error * 3) / 16;
          gray[i + width] += (error * 5) / 16;
          if (x + 1 < width) gray[i + width + 1] += error / 16;
        }
      }
    }
  }
  if (padding) {
    for (let y = 0; y < height; y++)
      for (let x = 0; x < width; x++) {
        if (
          x < padding ||
          y < padding ||
          x >= width - padding ||
          y >= height - padding
        )
          bits[y * width + x] = 0;
      }
  }
  return bits;
}

export function pack(bits, width, height) {
  dimensions(width, height);
  if (bits.length !== width * height)
    throw new Error("Bitmap dimensions and pixel count do not match.");
  const rowBytes = Math.ceil(width / 8);
  const bytes = new Uint8Array(rowBytes * height);
  for (let y = 0; y < height; y++)
    for (let x = 0; x < width; x++) {
      if (bits[y * width + x])
        bytes[y * rowBytes + Math.floor(x / 8)] |= 0x80 >> (x & 7);
    }
  return bytes;
}

export function header(bytes, width, height, name) {
  dimensions(width, height);
  validateName(name);
  if (bytes.length !== Math.ceil(width / 8) * height)
    throw new Error("Packed bitmap length does not match its dimensions.");
  const lines = [];
  for (let i = 0; i < bytes.length; i += 12) {
    lines.push(
      "    " +
        Array.from(
          bytes.slice(i, i + 12),
          (b) => "0x" + b.toString(16).padStart(2, "0"),
        ).join(", ") +
        ",",
    );
  }
  return `#pragma once\n#include <IndurUI.h>\n\nstatic const uint8_t ${name}_data[] INDURUI_PROGMEM = {\n${lines.join("\n")}\n};\nstatic const indur::Icon ${name}(${name}_data, ${width}, ${height}, indur::Memory::Flash);\n`;
}

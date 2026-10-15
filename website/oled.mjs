export function parseFrame(input) {
  let frame = input;
  if (typeof input === "string") {
    if (input.length > 65536)
      throw new Error("Choose a capture smaller than 64 KB.");
    const lines = input.trim().split(/\r?\n/);
    const serial = lines
      .filter((line) => line.startsWith("INDURUI_FRAME "))
      .at(-1);
    try {
      frame = JSON.parse(serial ? serial.slice(14) : input);
    } catch {
      throw new Error(
        "Paste a frame JSON object or a complete INDURUI_FRAME line.",
      );
    }
  }
  if (!frame || frame.format !== "indurui-ssd1306-v1")
    throw new Error("This is not an IndurUI SSD1306 capture.");
  const { width, height, data } = frame;
  if (
    !Number.isInteger(width) ||
    !Number.isInteger(height) ||
    width < 1 ||
    width > 256 ||
    height < 8 ||
    height > 128 ||
    height % 8
  ) {
    throw new Error(
      "The capture must be 1–256 pixels wide and 8–128 pixels high, in eight-pixel pages.",
    );
  }
  if (
    typeof data !== "string" ||
    !/^[0-9a-f]+$/i.test(data) ||
    data.length !== (width * height) / 4
  ) {
    throw new Error("The capture byte count does not match its dimensions.");
  }
  const bytes = Uint8Array.from(data.match(/../g), (hex) => parseInt(hex, 16));
  const bits = new Uint8Array(width * height);
  for (let y = 0; y < height; y++)
    for (let x = 0; x < width; x++) {
      bits[y * width + x] =
        (bytes[x + Math.floor(y / 8) * width] >> (y & 7)) & 1;
    }
  return {
    width,
    height,
    bytes,
    bits,
    source: { format: frame.format, width, height, data: data.toLowerCase() },
  };
}

export function paintFrame(canvas, frame) {
  canvas.width = frame.width;
  canvas.height = frame.height;
  const context = canvas.getContext("2d");
  const image = context.createImageData(frame.width, frame.height);
  for (let i = 0; i < frame.bits.length; i++) {
    const color = frame.bits[i] ? 255 : 0;
    image.data.set([color, color, color, 255], i * 4);
  }
  context.putImageData(image, 0, 0);
}

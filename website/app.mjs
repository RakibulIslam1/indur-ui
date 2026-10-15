import {
  dimensions,
  fitRect,
  monochrome,
  pack,
  header,
  validateName,
} from "./bitmap.mjs";

const $ = (id) => document.getElementById(id);
const bitmapCanvas = $("bitmap-canvas");
const menuCanvas = $("menu-canvas");
const work = document.createElement("canvas");
const controls = [
  "size-preset",
  "bitmap-width",
  "bitmap-height",
  "fit",
  "padding",
  "threshold",
  "invert",
  "dither",
];
const state = {
  image: null,
  result: null,
  code: "",
  view: "bitmap",
  loadId: 0,
};

function feedback(message, kind = "") {
  $("feedback").textContent = message;
  $("feedback").className = "feedback" + (kind ? " " + kind : "");
}

function sample(kind) {
  const canvas = document.createElement("canvas");
  canvas.width = canvas.height = 32;
  const context = canvas.getContext("2d");
  context.fillStyle = "#fff";
  if (kind === "robot") {
    [
      [14, 0, 6, 4],
      [16, 4, 2, 4],
      [8, 8, 16, 2],
      [6, 10, 2, 16],
      [24, 10, 2, 16],
      [8, 26, 16, 2],
      [0, 12, 4, 14],
      [28, 12, 4, 14],
      [10, 14, 4, 6],
      [18, 14, 4, 6],
      [12, 22, 8, 2],
    ].forEach((rect) => context.fillRect(...rect));
  } else {
    context.beginPath();
    const points =
      kind === "bolt"
        ? [
            [16, 2],
            [27, 2],
            [18, 13],
            [27, 13],
            [10, 31],
            [14, 18],
            [6, 18],
          ]
        : [
            [4, 6],
            [12, 6],
            [16, 10],
            [20, 6],
            [28, 6],
            [32, 10],
            [32, 18],
            [16, 32],
            [0, 18],
            [0, 10],
          ];
    points.forEach(([x, y], i) =>
      i ? context.lineTo(x, y) : context.moveTo(x, y),
    );
    context.closePath();
    context.fill();
  }
  return canvas;
}

function useSample(kind) {
  state.loadId++;
  state.image = sample(kind);
  $("upload-help").textContent =
    `Demo: ${kind === "robot" ? "Rover" : kind === "bolt" ? "Lightning" : "Heart"} · try your own image`;
  document.querySelectorAll("[data-demo]").forEach((button) => {
    const selected = button.dataset.demo === kind;
    button.classList.toggle("selected", selected);
    button.setAttribute("aria-pressed", String(selected));
  });
  render();
  feedback("Sample loaded. Try a new size or make it your own.");
}

function options() {
  const custom = $("size-preset").value === "custom";
  const width = Number(
    custom ? $("bitmap-width").value : $("size-preset").value,
  );
  const height = Number(
    custom ? $("bitmap-height").value : $("size-preset").value,
  );
  dimensions(width, height);
  const maxPadding = Math.floor((Math.min(width, height) - 1) / 2);
  $("padding").max = maxPadding;
  const requestedPadding = Number($("padding").value);
  if (!Number.isInteger(requestedPadding) || requestedPadding < 0)
    throw new Error("Padding must be a positive whole number or zero.");
  if (requestedPadding > maxPadding) $("padding").value = maxPadding;
  return {
    width,
    height,
    padding: Math.min(requestedPadding, maxPadding),
    fit: $("fit").value,
    threshold: Number($("threshold").value),
    invert: $("invert").checked,
    dither: $("dither").checked,
  };
}

function paintBitmap(bits, width, height) {
  bitmapCanvas.width = width;
  bitmapCanvas.height = height;
  const context = bitmapCanvas.getContext("2d");
  const pixels = context.createImageData(width, height);
  for (let i = 0; i < bits.length; i++) {
    const value = bits[i] ? 255 : 0;
    pixels.data.set([value, value, value, 255], i * 4);
  }
  context.putImageData(pixels, 0, 0);
  $("bitmap-surface").style.width =
    `${Math.min(260, (260 * width) / height) + 16}px`;
  $("pixel-grid").style.setProperty("--bitmap-columns", width);
  $("pixel-grid").style.setProperty("--bitmap-rows", height);
}

function paintMenu() {
  if (!state.result) return;
  const { bits, width, height } = state.result;
  const context = menuCanvas.getContext("2d");
  const ink = "#d9fff0";
  context.fillStyle = "#071b17";
  context.fillRect(0, 0, 128, 64);
  context.strokeStyle = ink;
  context.fillStyle = ink;
  context.lineWidth = 1;
  context.font = "7px monospace";
  const rect = fitRect(width, height, 32, 32);
  for (let y = 0; y < rect.height; y++)
    for (let x = 0; x < rect.width; x++) {
      const sourceX = Math.min(width - 1, Math.floor((x * width) / rect.width));
      const sourceY = Math.min(
        height - 1,
        Math.floor((y * height) / rect.height),
      );
      if (bits[sourceY * width + sourceX])
        context.fillRect(48 + rect.x + x, 4 + rect.y + y, 1, 1);
    }
  context.textAlign = "center";
  context.fillText("YOUR ROBOT", 64, 47);
  context.strokeRect(124.5, 4.5, 1, 37);
  context.fillRect(123, 5, 3, 5);
  context.fillRect(1, 54, 39, 9);
  context.fillRect(88, 54, 39, 9);
  context.strokeRect(44.5, 54.5, 38, 8);
  context.fillText("01/08", 64, 61);
  context.fillStyle = "#071b17";
  context.fillText("BACK", 20, 61);
  context.fillText("SELECT", 108, 61);
}

function updateHeader() {
  if (!state.result) return;
  try {
    const name = validateName($("icon-name").value.trim());
    state.code = header(
      state.result.bytes,
      state.result.width,
      state.result.height,
      name,
    );
    $("header-code").textContent = state.code;
    $("usage-code").textContent =
      `#include "${name}.h"\n\nui.setIcon(menuItem, ${name});`;
    $("icon-name").setAttribute("aria-invalid", "false");
    $("name-help").textContent = "The name you’ll use in your sketch.";
    $("download-header").disabled = $("copy-header").disabled = false;
  } catch (error) {
    state.code = "";
    $("header-code").textContent =
      "Choose a valid icon name to generate your header.";
    $("icon-name").setAttribute("aria-invalid", "true");
    $("name-help").textContent = error.message;
    $("download-header").disabled = $("copy-header").disabled = true;
  }
}

function render() {
  $("threshold-value").textContent = $("threshold").value;
  $("threshold").disabled = $("dither").checked;
  $("custom-size").hidden = $("size-preset").value !== "custom";
  if (!state.image) return;
  try {
    const opts = options();
    const sourceWidth = state.image.naturalWidth || state.image.width;
    const sourceHeight = state.image.naturalHeight || state.image.height;
    const rect = fitRect(
      sourceWidth,
      sourceHeight,
      opts.width,
      opts.height,
      opts.padding,
      opts.fit,
    );
    work.width = opts.width;
    work.height = opts.height;
    const context = work.getContext("2d", { willReadFrequently: true });
    context.fillStyle = "#000";
    context.fillRect(0, 0, opts.width, opts.height);
    context.save();
    context.beginPath();
    context.rect(
      opts.padding,
      opts.padding,
      opts.width - 2 * opts.padding,
      opts.height - 2 * opts.padding,
    );
    context.clip();
    context.imageSmoothingEnabled = true;
    context.imageSmoothingQuality = "high";
    context.drawImage(state.image, rect.x, rect.y, rect.width, rect.height);
    context.restore();
    const rgba = context.getImageData(0, 0, opts.width, opts.height).data;
    const bits = monochrome(rgba, { ...opts, contentRect: rect });
    const bytes = pack(bits, opts.width, opts.height);
    state.result = { ...opts, bits, bytes };
    paintBitmap(bits, opts.width, opts.height);
    paintMenu();
    $("dimensions-label").textContent = `${opts.width} × ${opts.height} px`;
    $("byte-count").replaceChildren(
      document.createTextNode(String(bytes.length)),
      Object.assign(document.createElement("span"), { textContent: " B" }),
    );
    $("lit-count").textContent =
      `${Math.round((bits.reduce((sum, bit) => sum + bit, 0) / bits.length) * 100)}%`;
    $("download-png").disabled = false;
    $("bitmap-width").setAttribute("aria-invalid", "false");
    $("bitmap-height").setAttribute("aria-invalid", "false");
    updateHeader();
    feedback(
      opts.dither
        ? "Dithering is on. The threshold slider is paused."
        : "Looking good. Your bitmap is ready to export.",
    );
  } catch (error) {
    state.result = null;
    state.code = "";
    $("download-header").disabled =
      $("download-png").disabled =
      $("copy-header").disabled =
        true;
    $("header-code").textContent =
      "Adjust the image settings to generate your header.";
    $("dimensions-label").textContent = "Preview paused";
    $("byte-count").textContent = "—";
    $("lit-count").textContent = "—";
    for (const id of ["bitmap-width", "bitmap-height"])
      $(id).setAttribute(
        "aria-invalid",
        String(!$(id).validity.valid || !$(id).value),
      );
    feedback(error.message, "error");
  }
}

async function openFile(file) {
  if (!file) return;
  const id = ++state.loadId;
  if (file.size > 20 * 1024 * 1024) {
    feedback("Please choose an image smaller than 20 MB.", "error");
    return;
  }
  if (
    !/\.(png|jpe?g|svg|webp|gif|bmp)$/i.test(file.name) &&
    !/^image\/(png|jpeg|svg\+xml|webp|gif|bmp|x-ms-bmp)$/.test(file.type)
  ) {
    feedback("Choose a PNG, JPG, SVG, WebP, GIF, or BMP image.", "error");
    return;
  }
  const url = URL.createObjectURL(file);
  try {
    const image = new Image();
    image.src = url;
    await image.decode();
    if (id !== state.loadId) return;
    if (image.naturalWidth * image.naturalHeight > 16000000)
      throw new Error("Use an image with no more than 16 million pixels.");
    state.image = image;
    $("upload-help").textContent =
      `${file.name} · ${image.naturalWidth} × ${image.naturalHeight} px`;
    document.querySelectorAll("[data-demo]").forEach((button) => {
      button.classList.remove("selected");
      button.setAttribute("aria-pressed", "false");
    });
    render();
  } catch (error) {
    if (id === state.loadId)
      feedback(
        error.message.includes("16 million")
          ? error.message
          : "This image could not be read. Try another file; your previous image is still here.",
        "error",
      );
  } finally {
    URL.revokeObjectURL(url);
  }
}

function download(blob, filename) {
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 2000);
}

controls.forEach((id) => $(id).addEventListener("input", render));
$("icon-name").addEventListener("input", updateHeader);
$("show-grid").addEventListener("change", () => {
  $("pixel-grid").hidden = !$("show-grid").checked;
});
$("image-file").addEventListener("change", (event) => {
  openFile(event.target.files[0]);
  event.target.value = "";
});
document
  .querySelectorAll("[data-demo]")
  .forEach((button) =>
    button.addEventListener("click", () => useSample(button.dataset.demo)),
  );
for (const id of ["bitmap", "menu"])
  $(id + "-tab").addEventListener("click", () => {
    state.view = id;
    for (const mode of ["bitmap", "menu"]) {
      $(mode + "-surface").hidden = mode !== id;
      $(mode + "-tab").classList.toggle("selected", mode === id);
      $(mode + "-tab").setAttribute("aria-pressed", String(mode === id));
    }
    $("show-grid").disabled = id === "menu";
  });
const dropzone = $("dropzone");
for (const name of ["dragenter", "dragover"])
  dropzone.addEventListener(name, (event) => {
    event.preventDefault();
    dropzone.classList.add("dragover");
  });
for (const name of ["dragleave", "drop"])
  dropzone.addEventListener(name, (event) => {
    event.preventDefault();
    dropzone.classList.remove("dragover");
  });
dropzone.addEventListener("drop", (event) =>
  openFile(event.dataTransfer.files[0]),
);
window.addEventListener("dragover", (event) => event.preventDefault());
window.addEventListener("drop", (event) => event.preventDefault());
$("download-header").addEventListener("click", () => {
  if (!state.code) return;
  const name = validateName($("icon-name").value.trim());
  download(
    new Blob([state.code], { type: "text/x-c++hdr;charset=utf-8" }),
    name + ".h",
  );
  feedback(
    `${name}.h downloaded. Add it next to your Arduino sketch.`,
    "success",
  );
});
$("download-png").addEventListener("click", () => {
  if (!state.result) return;
  const width = bitmapCanvas.width;
  const height = bitmapCanvas.height;
  let name = "indur-bitmap";
  try {
    name = validateName($("icon-name").value.trim());
  } catch {
    /* PNG export does not need a C++ identifier. */
  }
  bitmapCanvas.toBlob((blob) => {
    if (!blob)
      return feedback(
        "The PNG could not be created. Please try again.",
        "error",
      );
    download(blob, `${name}-${width}x${height}.png`);
    feedback("Native-size preview PNG downloaded.", "success");
  }, "image/png");
});
$("copy-header").addEventListener("click", async () => {
  if (!state.code) return;
  try {
    await navigator.clipboard.writeText(state.code);
    feedback("Header copied. Your next build is one step closer.", "success");
    $("copy-header").querySelector("span").textContent = "Copied";
    setTimeout(() => {
      $("copy-header").querySelector("span").textContent = "Copy";
    }, 1800);
  } catch {
    const selection = window.getSelection();
    const range = document.createRange();
    range.selectNodeContents($("header-code"));
    selection.removeAllRanges();
    selection.addRange(range);
    feedback(
      "Clipboard access is blocked. The header is selected; use your device’s Copy command.",
    );
  }
});
$("reset").addEventListener("click", () => {
  $("size-preset").value = "32";
  $("bitmap-width").value = $("bitmap-height").value = "32";
  $("fit").value = "contain";
  $("padding").value = "0";
  $("threshold").value = "128";
  $("invert").checked = $("dither").checked = $("show-grid").checked = false;
  $("pixel-grid").hidden = true;
  $("icon-name").value = "robotIcon";
  $("bitmap-tab").click();
  useSample("robot");
  feedback("A fresh start. Your default icon is ready.");
});
useSample("robot");

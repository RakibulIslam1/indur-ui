import { parseFrame, paintFrame } from "./oled.mjs";

const $ = (id) => document.getElementById(id);
const screens = [
  ["dashboard", "Live dashboard"],
  ["menu", "Icon menu"],
  ["settings", "Settings list"],
  ["editor", "Numeric editor"],
  ["sensors", "Sensor arc"],
  ["telemetry", "Telemetry page"],
  ["hold", "Hold feedback"],
  ["alert", "Persistent alert"],
  ["confirmation", "Confirmation"],
  ["progress", "Progress"],
  ["notification", "Notification"],
  ["splash", "Splash screen"],
];
let current = null;
let screenRequest = 0;
let port = null,
  reader = null,
  readTask = null,
  closing = false;
function status(text, error = false) {
  $("capture-status").textContent = text;
  $("capture-status").classList.toggle("error", error);
}
function show(frame, label) {
  current = frame;
  paintFrame($("capture-canvas"), frame);
  $("capture-title").textContent = label;
  $("capture-info").textContent =
    `${frame.width} × ${frame.height} · ${frame.bytes.length} framebuffer bytes`;
  $("capture-download").disabled = $("capture-copy").disabled = false;
}
function importText(text, label = "Your robot’s OLED") {
  try {
    const frame = parseFrame(text);
    ++screenRequest;
    show(frame, label);
    document.querySelectorAll("[data-screen]").forEach((button) => {
      button.classList.remove("selected");
      button.setAttribute("aria-pressed", "false");
    });
    status("Capture loaded. Every pixel comes directly from the framebuffer.");
  } catch (error) {
    status(error.message, true);
  }
}
async function loadScreen(name, label) {
  const request = ++screenRequest;
  try {
    const response = await fetch(
      new URL(`./frames/${name}.json`, import.meta.url),
    );
    if (!response.ok)
      throw new Error("The capture could not be loaded. Please try again.");
    const frame = parseFrame(await response.json());
    if (request !== screenRequest) return;
    show(frame, label);
    status(
      "Real library rendering with fixed demo values. Not a photograph or an SVG redraw.",
    );
    document.querySelectorAll("[data-screen]").forEach((button) => {
      const selected = button.dataset.screen === name;
      button.classList.toggle("selected", selected);
      button.setAttribute("aria-pressed", String(selected));
    });
  } catch (error) {
    status(error.message, true);
  }
}
for (const [name, label] of screens) {
  const button = document.createElement("button");
  button.type = "button";
  button.dataset.screen = name;
  button.setAttribute("aria-pressed", "false");
  const image = document.createElement("img");
  image.src = `./frames/${name}-4x.png`;
  image.alt = `${label}: exact IndurUI 128 by 64 framebuffer`;
  image.width = 512;
  image.height = 256;
  image.loading = "lazy";
  const title = document.createElement("span");
  title.textContent = label;
  button.append(image, title);
  button.addEventListener("click", () => loadScreen(name, label));
  $("screen-gallery").append(button);
}
$("capture-import").addEventListener("click", () =>
  importText($("capture-input").value),
);
$("capture-file").addEventListener("change", async (event) => {
  const file = event.target.files[0];
  event.target.value = "";
  if (!file) return;
  if (file.size > 65536)
    return status("Choose a capture smaller than 64 KB.", true);
  importText(await file.text(), file.name);
});
$("capture-download").addEventListener("click", () => {
  if (!current) return;
  const scale = Number($("capture-scale").value),
    width = current.width,
    height = current.height;
  const output = document.createElement("canvas");
  output.width = width * scale;
  output.height = height * scale;
  const context = output.getContext("2d");
  context.imageSmoothingEnabled = false;
  context.drawImage($("capture-canvas"), 0, 0, output.width, output.height);
  output.toBlob((blob) => {
    if (!blob) return status("PNG export failed. Please try again.", true);
    const url = URL.createObjectURL(blob),
      link = document.createElement("a");
    link.href = url;
    link.download = `indurui-oled-${width}x${height}-${scale}x.png`;
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 2000);
    status(`Screenshot saved at ${scale}× with sharp, unfiltered pixels.`);
  }, "image/png");
});
$("capture-copy").addEventListener("click", async () => {
  if (!current) return;
  const text = JSON.stringify(current.source);
  try {
    await navigator.clipboard.writeText(text);
    status("Raw capture JSON copied.");
  } catch {
    $("capture-input").value = text;
    $("capture-input").focus();
    $("capture-input").select();
    status("Capture selected below; use your device’s Copy command.");
  }
});
function serialButtons() {
  $("serial-connect").textContent = port
    ? "Disconnect USB"
    : "Connect robot USB";
  $("serial-capture").disabled = !port;
}
async function disconnect() {
  closing = true;
  try {
    if (reader) await reader.cancel();
    if (readTask) await readTask;
    if (port) await port.close();
  } catch {
    /* An unplugged device may already be closed. */
  }
  port = reader = readTask = null;
  closing = false;
  serialButtons();
}
async function readFrames() {
  const decoder = new TextDecoder();
  let pending = "";
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      pending += decoder.decode(value, { stream: true });
      let end;
      while ((end = pending.indexOf("\n")) >= 0) {
        const line = pending.slice(0, end).trim();
        pending = pending.slice(end + 1);
        if (line.startsWith("INDURUI_FRAME "))
          importText(line, "Captured from your robot");
      }
      if (pending.length > 65536) {
        pending = "";
        status("Serial data was too long. Request another frame.", true);
      }
    }
  } catch (error) {
    if (!closing) status(`USB connection ended: ${error.message}`, true);
  } finally {
    if (reader) reader.releaseLock();
    reader = null;
    if (!closing && port) {
      const ended = port;
      port = null;
      readTask = null;
      try {
        await ended.close();
      } catch {}
      serialButtons();
    }
  }
}
$("serial-connect").addEventListener("click", async () => {
  if (port) {
    await disconnect();
    status("USB disconnected. Imported and demo captures remain available.");
    return;
  }
  $("serial-connect").disabled = true;
  try {
    const selected = await navigator.serial.requestPort();
    await selected.open({ baudRate: 115200 });
    port = selected;
    reader = port.readable.getReader();
    readTask = readFrames();
    status("USB connected. Choose Capture OLED to read the current screen.");
  } catch (error) {
    status(
      error.name === "NotFoundError"
        ? "No USB port selected."
        : `Could not connect: ${error.message}`,
      true,
    );
  } finally {
    $("serial-connect").disabled = false;
    serialButtons();
  }
});
$("serial-capture").addEventListener("click", async () => {
  if (!port || !port.writable) return;
  const writer = port.writable.getWriter();
  try {
    await writer.write(new TextEncoder().encode("p\n"));
    status(
      "Frame requested. The mini test firmware sends the current OLED buffer.",
    );
  } catch (error) {
    status(`Capture request failed: ${error.message}`, true);
  } finally {
    writer.releaseLock();
  }
});
if (!("serial" in navigator)) {
  $("serial-connect").disabled = true;
  $("serial-help").textContent =
    "USB capture needs desktop Chrome or Edge. You can import capture JSON in this browser.";
} else
  navigator.serial.addEventListener("disconnect", (event) => {
    if (event.target === port)
      disconnect().then(() => status("Robot unplugged. Reconnect when ready."));
  });
loadScreen("dashboard", "Live dashboard");

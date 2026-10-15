#!/usr/bin/env python3
"""Convert images to row-padded, MSB-first IndurUI icons; CLI or Tk desktop UI."""
from __future__ import annotations

import argparse
import re
from pathlib import Path
from PIL import Image, ImageOps

PRESETS = (12, 16, 24, 32, 48, 64)


def dimensions(value: str) -> tuple[int, int]:
    parts = value.lower().split("x")
    try:
        width, height = (int(parts[0]), int(parts[-1])) if len(parts) <= 2 else (0, 0)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("Use SIZE or WIDTHxHEIGHT, for example 32x24") from exc
    if not (1 <= width <= 128 and 1 <= height <= 128):
        raise argparse.ArgumentTypeError("Each dimension must be between 1 and 128")
    return width, height


def convert(image: Image.Image, size: tuple[int, int] = (32, 32), fit: str = "contain",
            threshold: int = 128, invert: bool = False, dither: bool = False,
            padding: int = 0) -> Image.Image:
    """Return a black-background 1-bit image. White pixels light up on the OLED.

    Alpha is composited onto black before optional inversion. Dithering replaces
    thresholding. Padding reserves an unlit border after fitting/inversion.
    """
    width, height = size
    if not (1 <= width <= 128 and 1 <= height <= 128):
        raise ValueError("Dimensions must be between 1 and 128")
    if not 0 <= threshold <= 255:
        raise ValueError("Threshold must be between 0 and 255")
    if padding < 0 or 2 * padding >= min(size):
        raise ValueError("Padding must leave at least one image pixel")
    if fit not in ("contain", "cover", "stretch"):
        raise ValueError("Fit must be contain, cover, or stretch")
    source = ImageOps.exif_transpose(image).convert("RGBA")
    background = Image.new("RGBA", source.size, (0, 0, 0, 255))
    background.alpha_composite(source)
    gray = background.convert("L")
    if invert:
        gray = ImageOps.invert(gray)
    inside = (width - 2 * padding, height - 2 * padding)
    if fit == "contain":
        fitted = ImageOps.contain(gray, inside, Image.Resampling.LANCZOS)
    elif fit == "cover":
        fitted = ImageOps.fit(gray, inside, Image.Resampling.LANCZOS)
    else:
        fitted = gray.resize(inside, Image.Resampling.LANCZOS)
    canvas = Image.new("L", size, 0)
    canvas.paste(fitted, ((width - fitted.width) // 2, (height - fitted.height) // 2))
    if dither:
        result = canvas.convert("1", dither=Image.Dither.FLOYDSTEINBERG)
    else:
        result = canvas.point(lambda p: 255 if p >= threshold else 0, mode="1")
    if padding:
        result.paste(0, (0, 0, width, padding))
        result.paste(0, (0, height - padding, width, height))
        result.paste(0, (0, 0, padding, height))
        result.paste(0, (width - padding, 0, width, height))
    return result


def pack(image: Image.Image) -> bytes:
    """One byte per eight horizontal pixels; each row starts a new byte."""
    mono = image.convert("1")
    result = bytearray()
    for y in range(mono.height):
        for start in range(0, mono.width, 8):
            value = 0
            for bit in range(8):
                if start + bit < mono.width and mono.getpixel((start + bit, y)):
                    value |= 0x80 >> bit
            result.append(value)
    return bytes(result)


def header(image: Image.Image, name: str) -> str:
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", name) or "__" in name:
        raise ValueError("Name must start with a letter and contain letters, digits, or single underscores")
    if name in {"class", "auto", "int", "float", "double", "bool", "void", "static", "const",
                "return", "namespace", "template", "new", "delete", "if", "else", "while",
                "for", "switch", "case", "default", "true", "false", "nullptr", "operator",
                "struct", "enum", "union", "using", "virtual", "public", "private", "protected",
                "try", "catch", "throw", "this", "sizeof", "volatile", "extern", "typedef",
                "char", "short", "long", "signed", "unsigned", "inline", "constexpr", "do",
                "break", "continue", "goto", "register", "mutable", "explicit", "friend",
                "alignas", "alignof", "decltype", "noexcept", "static_assert", "thread_local",
                "typeid", "typename", "wchar_t", "char16_t", "char32_t", "and", "or", "not",
                "bitand", "bitor", "xor", "compl", "and_eq", "or_eq", "xor_eq", "not_eq",
                "const_cast", "dynamic_cast", "reinterpret_cast", "static_cast", "export"}:
        raise ValueError("Name cannot be a C++ keyword")
    data = pack(image)
    lines = ["    " + ", ".join(f"0x{b:02x}" for b in data[i:i + 12]) + ","
             for i in range(0, len(data), 12)]
    return ("#pragma once\n#include <IndurUI.h>\n\n"
            f"static const uint8_t {name}_data[] INDURUI_PROGMEM = {{\n"
            + "\n".join(lines) + "\n};\n"
            f"static const indur::Icon {name}({name}_data, {image.width}, {image.height}, indur::Memory::Flash);\n")


def gui() -> None:
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    from PIL import ImageTk

    root = tk.Tk()
    root.title("IndurUI · Bitmap Studio")
    root.minsize(660, 530)
    source = None
    preview = None
    source_path = None
    photo = None
    name = tk.StringVar(value="robotIcon")
    size = tk.StringVar(value="32x32")
    fit = tk.StringVar(value="contain")
    threshold = tk.IntVar(value=128)
    padding = tk.IntVar(value=0)
    invert = tk.BooleanVar(value=False)
    dither = tk.BooleanVar(value=False)
    info = tk.StringVar(value="Open a PNG, JPEG, BMP, GIF, or WebP image")
    outer = ttk.Frame(root, padding=20)
    outer.pack(fill="both", expand=True)
    ttk.Label(outer, text="IndurUI Bitmap Studio", font=("Segoe UI", 20, "bold")).pack(anchor="w")
    ttk.Label(outer, text="A custom icon, ready for your OLED menu.").pack(anchor="w", pady=(0, 14))
    body = ttk.Frame(outer)
    body.pack(fill="both", expand=True)
    controls = ttk.Frame(body, width=280)
    controls.pack(side="left", fill="y", padx=(0, 20))
    display = tk.Label(body, bg="#101820", width=40, height=16)
    display.pack(side="right", fill="both", expand=True)

    def update(*_):
        nonlocal preview, photo
        if source is None:
            return
        try:
            preview = convert(source, dimensions(size.get()), fit.get(), threshold.get(),
                              invert.get(), dither.get(), padding.get())
            factor = max(1, min(240 // preview.width, 240 // preview.height))
            scaled = preview.resize((preview.width * factor, preview.height * factor), Image.Resampling.NEAREST)
            photo = ImageTk.PhotoImage(scaled)
            display.configure(image=photo, width=240, height=240)
            info.set(f"{preview.width} × {preview.height} px · {len(pack(preview))} bytes · white = lit pixel")
        except (ValueError, tk.TclError, argparse.ArgumentTypeError) as exc:
            preview = None
            info.set(str(exc))

    def open_image():
        nonlocal source, source_path
        path = filedialog.askopenfilename(filetypes=[("Images", "*.png *.jpg *.jpeg *.bmp *.gif *.webp"), ("All files", "*.*")])
        if path:
            try:
                with Image.open(path) as image:
                    source = image.copy()
                source_path = Path(path).resolve()
                update()
            except (OSError, ValueError) as exc:
                messagebox.showerror("Cannot open image", str(exc))

    def save():
        if preview is None:
            messagebox.showinfo("Choose an image", "Open an image and choose valid dimensions first.")
            return
        try:
            result = header(preview, name.get())
            path = filedialog.asksaveasfilename(defaultextension=".h", initialfile=name.get() + ".h", filetypes=[("Arduino header", "*.h")])
            if path:
                if Path(path).resolve() == source_path:
                    raise ValueError("Choose a different filename from the input image")
                Path(path).write_text(result, encoding="utf-8")
                info.set(f"Saved {Path(path).name} · use ui.setIcon(item, {name.get()});")
        except (ValueError, OSError) as exc:
            messagebox.showerror("Cannot save header", str(exc))

    ttk.Button(controls, text="Open image…", command=open_image).pack(fill="x", pady=(0, 12))
    for label, variable, values in (("Bitmap size (WxH)", size, [f"{n}x{n}" for n in PRESETS]),
                                     ("Fit", fit, ["contain", "cover", "stretch"])):
        ttk.Label(controls, text=label).pack(anchor="w")
        box = ttk.Combobox(controls, textvariable=variable, values=values, width=24)
        box.pack(fill="x", pady=(2, 10))
        box.bind("<<ComboboxSelected>>", update)
        box.bind("<KeyRelease>", update)
    ttk.Label(controls, text="C++ icon name").pack(anchor="w")
    ttk.Entry(controls, textvariable=name).pack(fill="x", pady=(2, 10))
    ttk.Label(controls, text="Padding (pixels)").pack(anchor="w")
    ttk.Spinbox(controls, from_=0, to=32, textvariable=padding, command=update, width=8).pack(anchor="w")
    ttk.Label(controls, text="Threshold (dithering overrides this)").pack(anchor="w", pady=(10, 0))
    tk.Scale(controls, from_=0, to=255, orient="horizontal", variable=threshold, command=update).pack(fill="x")
    ttk.Checkbutton(controls, text="Invert image", variable=invert, command=update).pack(anchor="w")
    ttk.Checkbutton(controls, text="Floyd–Steinberg dithering", variable=dither, command=update).pack(anchor="w")
    ttk.Button(controls, text="Save Arduino header…", command=save).pack(fill="x", pady=(16, 0))
    ttk.Label(outer, textvariable=info, wraplength=620).pack(anchor="w", pady=(14, 0))
    root.mainloop()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", nargs="?", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--name", default="robotIcon")
    parser.add_argument("--size", type=dimensions, default=(32, 32))
    parser.add_argument("--fit", choices=["contain", "cover", "stretch"], default="contain")
    parser.add_argument("--threshold", type=int, default=128)
    parser.add_argument("--padding", type=int, default=0)
    parser.add_argument("--invert", action="store_true")
    parser.add_argument("--dither", action="store_true")
    parser.add_argument("--preview", type=Path, help="Optional native-resolution preview PNG")
    parser.add_argument("--gui", action="store_true")
    args = parser.parse_args(argv)
    if args.gui or args.input is None:
        gui()
        return 0
    if not args.output:
        parser.error("--output is required in CLI mode")
    try:
        paths = [args.input.resolve(), args.output.resolve()]
        if args.preview:
            paths.append(args.preview.resolve())
        if len(set(paths)) != len(paths):
            raise ValueError("Input, header output, and preview must use different paths")
        with Image.open(args.input) as image:
            result = convert(image, args.size, args.fit, args.threshold, args.invert, args.dither, args.padding)
        output = header(result, args.name)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output, encoding="utf-8")
        if args.preview:
            args.preview.parent.mkdir(parents=True, exist_ok=True)
            result.save(args.preview, format="PNG")
        print(f"{args.output}: {result.width}x{result.height}, {len(pack(result))} bytes; ui.setIcon(item, {args.name});")
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

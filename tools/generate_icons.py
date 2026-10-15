"""Regenerate the built-in geometric icon set and example using Pillow."""
from pathlib import Path
from PIL import Image, ImageDraw
from bitmap_converter import header

ROOT = Path(__file__).resolve().parents[1]


def make_icon(name):
    image = Image.new("1", (16, 16))
    d = ImageDraw.Draw(image)
    if name == "settings":
        for y, x in ((3, 5), (7, 10), (12, 7)):
            d.line((1, y, 14, y), fill=1)
            d.rectangle((x - 1, y - 2, x + 1, y + 2), fill=1)
    elif name == "run":
        d.polygon(((4, 2), (13, 8), (4, 14)), fill=1)
    elif name == "sensors":
        for x, h in ((2, 5), (6, 11), (10, 8)):
            d.rectangle((x, 14 - h, x + 2, 14), outline=1)
    elif name == "info":
        d.ellipse((1, 1, 14, 14), outline=1)
        d.rectangle((7, 7, 8, 11), fill=1)
        d.rectangle((7, 4, 8, 5), fill=1)
    elif name == "tools":
        d.line((3, 12, 11, 4), fill=1, width=3)
        d.arc((7, 0, 15, 8), 0, 280, fill=1, width=2)
        d.ellipse((1, 10, 5, 14), outline=1)
    elif name == "storage":
        d.rectangle((2, 1, 13, 14), outline=1)
        d.rectangle((5, 1, 10, 5), outline=1)
        d.rectangle((4, 9, 11, 14), outline=1)
    elif name == "robot":
        d.rounded_rectangle((3, 4, 12, 13), radius=2, outline=1)
        d.rectangle((0, 6, 1, 12), fill=1)
        d.rectangle((14, 6, 15, 12), fill=1)
        for x in (5, 10):
            d.rectangle((x, 7, x, 8), fill=1)
        d.line((6, 11, 9, 11), fill=1)
        d.line((8, 1, 8, 4), fill=1)
        d.ellipse((7, 0, 9, 2), fill=1)
    elif name == "home":
        d.line(((1, 7), (8, 1), (14, 7)), fill=1, width=2)
        d.rectangle((3, 7, 12, 14), outline=1)
        d.rectangle((7, 10, 9, 14), outline=1)
    return image


def main():
    names = ("settings", "run", "sensors", "info", "tools", "storage", "robot", "home")
    source = "#pragma once\n#include <IndurUI.h>\n\nnamespace indur { namespace icons {\n"
    for name in names:
        generated = header(make_icon(name), name)
        generated = generated.split("\n\n", 1)[1]
        generated = generated[:generated.index("static const indur::Icon")]
        source += generated + f"inline Icon {name}() {{ return Icon({name}_data, 16, 16); }}\n\n"
    source += "}} // namespace indur::icons\n"
    (ROOT / "src/IndurUIIcons.h").write_text(source, encoding="utf-8")
    image = make_icon("robot").resize((32, 32), Image.Resampling.NEAREST)
    (ROOT / "docs/assets").mkdir(parents=True, exist_ok=True)
    image.save(ROOT / "docs/assets/robot-icon.png")
    (ROOT / "examples/CustomIcons/robotIcon.h").write_text(header(image, "robotIcon"), encoding="utf-8")


if __name__ == "__main__":
    main()

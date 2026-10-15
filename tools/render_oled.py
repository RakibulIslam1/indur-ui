"""Generate exact 128x64 UI captures using the real core and Adafruit_GFX 1.12.6.

Supply --gfx-dir pointing to that dependency's source folder. Use a compiler
environment with cl (Windows) or g++ (Linux/macOS). No display is required.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gfx-dir", type=Path)
    parser.add_argument("--check", action="store_true", help="Verify published JSON and PNG pixels without changing them")
    args = parser.parse_args()
    candidates = list((ROOT / ".pio/libdeps").glob("*/Adafruit GFX Library"))
    gfx = args.gfx_dir or (candidates[0] if candidates else None)
    if gfx is None or not (gfx / "Adafruit_GFX.cpp").is_file():
        parser.error("Provide --gfx-dir with the Adafruit GFX Library 1.12.6 source")
    gfx = gfx.resolve()
    if "version=1.12.6" not in (gfx / "library.properties").read_text():
        parser.error("Reproducible captures require Adafruit GFX Library 1.12.6")
    build = ROOT / ".build/oled-render"
    published = ROOT / "website/frames"
    output = build / "check-frames" if args.check else published
    build.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    sources = [ROOT / "tests/render_oled.cpp", ROOT / "src/IndurUI.cpp", gfx / "Adafruit_GFX.cpp"]
    includes = [ROOT / "tests/gfx-host", ROOT / "src", gfx]
    if sys.platform == "win32":
        exe = build / "render_oled.exe"
        command = ["cl", "/nologo", "/EHsc", "/std:c++14", "/D_CRT_SECURE_NO_WARNINGS"]
        command += ["/I" + str(p) for p in includes] + [str(p) for p in sources]
        command += ["/Fe:" + str(exe), "/Fo:" + str(build) + "/"]
    else:
        exe = build / "render_oled"
        command = ["g++", "-std=c++11", "-O2"]
        command += ["-I" + str(p) for p in includes] + [str(p) for p in sources] + ["-o", str(exe)]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(exe), str(output)], cwd=ROOT, check=True)
    for path in sorted(output.glob("*.json")):
        frame = json.loads(path.read_text())
        data = bytes.fromhex(frame["data"])
        assert len(data) == 1024
        image = Image.new("1", (128, 64), 0)
        for y in range(64):
            for x in range(128):
                if data[x + (y // 8) * 128] & (1 << (y & 7)):
                    image.putpixel((x, y), 255)
        image.save(path.with_suffix(".png"))
        image.resize((512, 256), Image.Resampling.NEAREST).save(path.with_name(path.stem + "-4x.png"))
    if args.check:
        for path in sorted(output.iterdir()):
            expected = published / path.name
            if path.suffix == ".json":
                assert json.loads(path.read_text()) == json.loads(expected.read_text()), path.name
            else:
                with Image.open(path) as actual, Image.open(expected) as original:
                    assert actual.size == original.size and actual.convert("1").tobytes() == original.convert("1").tobytes(), path.name
        print("Published JSON and PNG pixels match a fresh render")
    print("Generated eight exact framebuffer captures using IndurUI and unmodified Adafruit_GFX")


if __name__ == "__main__":
    main()

import importlib.util
import tempfile
import unittest
from pathlib import Path
from PIL import Image

spec = importlib.util.spec_from_file_location("converter", Path(__file__).parents[1] / "tools/bitmap_converter.py")
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)


class BitmapTests(unittest.TestCase):
    def test_row_padding(self):
        image = Image.new("1", (9, 2))
        for point in ((0, 0), (8, 0), (1, 1)):
            image.putpixel(point, 1)
        self.assertEqual(converter.pack(image), bytes([0x80, 0x80, 0x40, 0x00]))

    def test_size_and_transparency(self):
        image = Image.new("RGBA", (20, 10), (255, 255, 255, 255))
        result = converter.convert(image, (32, 32), padding=2)
        self.assertEqual(result.size, (32, 32))
        self.assertEqual(result.getpixel((16, 0)), 0)
        self.assertNotEqual(result.getpixel((16, 16)), 0)
        self.assertEqual(len(converter.pack(result)), 128)
        alpha = Image.new("RGBA", (2, 2), (255, 255, 255, 0))
        self.assertEqual(converter.pack(converter.convert(alpha, (2, 2))), b"\0\0")
        self.assertEqual(converter.pack(converter.convert(alpha, (2, 2), invert=True)), b"\xc0\xc0")

    def test_fitting_and_dithering(self):
        image = Image.new("L", (20, 10), 255)
        for fit in ("cover", "stretch"):
            self.assertEqual(converter.convert(image, (16, 16), fit).getextrema(), (255, 255))
        ramp = Image.new("L", (16, 16), 128)
        self.assertEqual(converter.convert(ramp, (16, 16), dither=True).getextrema(), (0, 255))
        for kwargs in ({"size": (0, 32)}, {"padding": 16}, {"threshold": 256}, {"fit": "bad"}):
            with self.assertRaises(ValueError):
                converter.convert(image, **kwargs)

    def test_headers(self):
        image = Image.new("1", (9, 2))
        text = converter.header(image, "customIcon")
        self.assertIn("customIcon_data, 9, 2", text)
        self.assertIn("INDURUI_PROGMEM", text)
        for name in ("class", "8bit", "bad-name", "a__b", "_reserved"):
            with self.assertRaises(ValueError):
                converter.header(image, name)

    def test_cli_and_collision(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)
            image = path / "input.png"
            Image.new("L", (10, 10), 255).save(image)
            self.assertEqual(converter.main([str(image), "-o", str(path / "icon.h"), "--preview", str(path / "preview.png")]), 0)
            self.assertTrue((path / "preview.png").is_file())
            with self.assertRaises(SystemExit):
                converter.main([str(image), "-o", str(image)])
            with Image.open(image) as original:
                self.assertEqual(original.size, (10, 10))


if __name__ == "__main__":
    unittest.main()

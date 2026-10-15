"""Exercise Tk open/preview/export without opening a visible interactive window."""
import importlib.util
from pathlib import Path
import tempfile
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image

spec = importlib.util.spec_from_file_location("converter", Path(__file__).parents[1] / "tools/bitmap_converter.py")
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)
original_tk = tk.Tk
errors = []

with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    Image.new("RGB", (48, 24), "white").save(path / "input.png")
    filedialog.askopenfilename = lambda **_: str(path / "input.png")
    filedialog.asksaveasfilename = lambda **_: str(path / "output.h")
    messagebox.showerror = lambda *args: errors.append(args)

    def widgets(root):
        for child in root.winfo_children():
            yield child
            yield from widgets(child)

    def make_root():
        root = original_tk()
        root.withdraw()
        def exercise():
            try:
                buttons = {widget.cget("text"): widget for widget in widgets(root) if widget.winfo_class() == "TButton"}
                buttons["Open image…"].invoke()
                buttons["Save Arduino header…"].invoke()
            except Exception as exc:
                errors.append(str(exc))
            finally:
                root.destroy()
        root.after(100, exercise)
        return root

    tk.Tk = make_root
    converter.gui()
    assert not errors, errors
    assert "robotIcon_data, 32, 32" in (path / "output.h").read_text()
    print("Bitmap Studio GUI open/preview/export passed")

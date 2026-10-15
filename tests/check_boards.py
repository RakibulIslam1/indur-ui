"""Check MCU eligibility using the real AVR device headers, without linking firmware."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    executable = "avr-g++.exe" if sys.platform == "win32" else "avr-g++"
    parser.add_argument("--compiler", type=Path, default=Path.home() / ".platformio/packages/toolchain-atmelavr/bin" / executable)
    args = parser.parse_args()
    cases = (("atmega2560", "AVR", True, ""),
             ("atmega328p", "AVR", False, "Classic Nano/Uno are unsupported"),
             ("atmega168", "AVR", False, "Classic Nano/Uno are unsupported"),
             ("atmega32u4", "AVR", False, "requires AVR >=8 KiB"),
             ("atmega2560", "SAMD", False, "Other architectures need a validated port"))
    for mcu, architecture, accepted, diagnostic in cases:
        command = [str(args.compiler), f"-mmcu={mcu}", "-std=gnu++11", "-fsyntax-only", "-x", "c++",
                   "-DARDUINO=10819", f"-DARDUINO_ARCH_{architecture}=1", "-I", str(ROOT / "src"), "-"]
        result = subprocess.run(command, input='#include "IndurUI.h"\n', text=True, capture_output=True)
        assert (result.returncode == 0) == accepted, result.stderr
        if diagnostic:
            assert diagnostic in result.stderr, result.stderr
        print(f"{mcu} / {architecture}: {'accepted' if accepted else 'rejected with expected explanation'}")


if __name__ == "__main__":
    main()

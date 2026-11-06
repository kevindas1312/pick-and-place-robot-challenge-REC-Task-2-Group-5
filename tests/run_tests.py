#!/usr/bin/env python3
"""Run the actual sketches against host hardware stubs; requires a C++ compiler.

This checks control logic, not board libraries, electrical wiring or timing.
"""
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix="robot-tests-") as build:
    for name in ("controller", "receiver"):
        output = Path(build) / f"{name}.so"
        subprocess.run([
            os.environ.get("CXX", "g++"), "-std=c++11", "-Wall", "-Wextra",
            "-Werror", "-shared", "-fPIC", "-nostdlib", f"-I{root}",
            str(root / f"{name}_test.cpp"), "-o", str(output),
        ], check=True)
        result = ctypes.CDLL(str(output)).run_tests()
        if result:
            raise SystemExit(f"FAIL: {name}_test.cpp:{result}")
        print(f"PASS: {name}")

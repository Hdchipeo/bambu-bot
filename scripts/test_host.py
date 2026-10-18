#!/usr/bin/env python3
"""Run real firmware state/parser tests on a host using system or ESP-IDF cJSON."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(argv):
    subprocess.run(argv, check=True, cwd=ROOT)


def main():
    with tempfile.TemporaryDirectory(prefix="bambu-tests-") as tmp:
        cc, cxx = os.environ.get("CC", "cc"), os.environ.get("CXX", "c++")
        idf = Path(os.environ.get("IDF_PATH", str(Path.home() / "esp/esp-idf")))
        cjson = idf / "components/json/cJSON"
        flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g"]
        if (cjson / "cJSON.c").exists():
            obj = str(Path(tmp) / "cjson.o")
            run([cc, *flags, "-c", str(cjson / "cJSON.c"), "-o", obj])
            includes, libs = ["-I", str(cjson)], [obj]
        else:
            includes = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "libcjson"], text=True))
            libs = shlex.split(subprocess.check_output(["pkg-config", "--libs", "libcjson"], text=True))
        exe = str(Path(tmp) / "printer_state_test")
        run([cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", *flags, *includes,
             "-I", "main/bambu", "tests/printer_state_test.cc", "main/bambu/printer_state.cc",
             *libs, "-o", exe])
        run([exe])
        run(["python3", "-m", "unittest", "discover", "-s", "tests", "-p", "test_*.py"])


if __name__ == "__main__":
    main()

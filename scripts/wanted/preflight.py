#!/usr/bin/env python3
"""Read-only host inventory; optionally write a JSON report to an explicit path.

Copyright (c) 2026 Wanted Engine contributors.
SPDX-License-Identifier: Apache-2.0 OR MIT
"""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


def capture(argv):
    try:
        result = subprocess.run(argv, capture_output=True, text=True, timeout=20, check=False)
        return {"exit_code": result.returncode, "output": (result.stdout + result.stderr).strip()[:8000]}
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"error": str(exc)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    workspace = args.workspace.resolve(strict=True)
    disk = shutil.disk_usage(workspace)
    report = {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "logical_cpus": os.cpu_count(),
        "workspace": str(workspace),
        "free_disk_gib": round(disk.free / 1024**3, 2),
        "o3de_source_disk_minimum_gib": 100,
        "source_disk_requirement_met": disk.free >= 100 * 1024**3,
        "tools": {},
    }
    for name, version_args in (
        ("git", ["--version"]), ("cmake", ["--version"]), ("ninja", ["--version"]),
        ("python3", ["--version"]), ("python", ["--version"]),
        ("clang++", ["--version"]), ("g++", ["--version"]), ("cl", []),
    ):
        location = shutil.which(name)
        report["tools"][name] = {"path": location, **(capture([location, *version_args]) if location else {})}
    if shutil.which("git"):
        report["git_lfs"] = capture(["git", "lfs", "version"])
    if platform.system() == "Windows":
        powershell = shutil.which("powershell")
        if powershell:
            command = (
                "[ordered]@{os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber;"
                "cpu=Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores;"
                "memoryBytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory;"
                "gpu=Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,AdapterRAM;"
                "sdk=Get-ChildItem 'HKLM:\\SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots' -ErrorAction SilentlyContinue | Select-Object PSChildName} | ConvertTo-Json -Depth 5"
            )
            report["windows_hardware"] = capture([powershell, "-NoProfile", "-NonInteractive", "-Command", command])
        program_files = os.environ.get("ProgramFiles(x86)")
        if program_files:
            vswhere = Path(program_files) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if vswhere.is_file():
                report["visual_studio"] = capture([str(vswhere), "-products", "*", "-format", "json"])
    elif platform.system() == "Linux":
        meminfo = Path("/proc/meminfo")
        if meminfo.is_file():
            report["memory"] = [line for line in meminfo.read_text().splitlines() if line.startswith(("MemTotal:", "MemAvailable:"))]
        report["gpu_device_nodes"] = [str(path) for path in Path("/dev/dri").glob("*")]
        if shutil.which("lscpu"):
            report["cpu"] = capture(["lscpu"])
    text = json.dumps(report, indent=2) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(text, encoding="utf-8")
    print(text, end="")


if __name__ == "__main__":
    main()

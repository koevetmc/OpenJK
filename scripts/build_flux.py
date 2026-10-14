import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
OFF = (
    "BuildMPEngine", "BuildMPRdVanilla", "BuildMPRend2", "BuildMPDed", "BuildMPUI",
    "BuildSPEngine", "BuildSPGame", "BuildSPRdVanilla", "BuildJK2SPEngine",
    "BuildJK2SPGame", "BuildJK2SPRdVanilla", "BuildTests",
)


def run(command, log):
    print("+ " + " ".join(map(str, command)), flush=True)
    with log.open("w", encoding="utf-8") as output:
        process = subprocess.Popen(
            list(map(str, command)), cwd=ROOT, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
        )
        warnings = []
        for line in process.stdout:
            output.write(line)
            print(line, end="", flush=True)
            if re.search(r"\bwarning(?:\s+[A-Z]\d+)?\s*:|CMake Warning", line, re.I):
                warnings.append(line.strip())
        code = process.wait()
    if code:
        raise RuntimeError(f"command failed ({code}); see {log}")
    if warnings:
        raise RuntimeError(f"{len(warnings)} warning diagnostics; see {log}")


def package(build, system):
    destination = ROOT / "build" / "deliverables" / system / "fluxboard"
    destination.mkdir(parents=True, exist_ok=True)
    suffix = ".dll" if system == "windows" else ".so"
    for stem, folder in (("jampgame", "game"), ("cgame", "cgame")):
        filename = stem + "x86_64" + suffix
        source = build / "Release" / filename if system == "windows" else build / "codemp" / folder / filename
        shutil.copy2(source, destination / filename)
    with zipfile.ZipFile(destination / "flux-assets.pk3", "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted((ROOT / "assets" / "flux").rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(ROOT / "assets" / "flux").as_posix())
    for filename in ("LICENSE.txt", "FLUXBOARD.md"):
        source = ROOT / filename
        if source.is_file():
            shutil.copy2(source, destination / filename)
    hashes = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(destination.iterdir()) if path.is_file() and path.name != "sha256.json"
    }
    (destination / "sha256.json").write_text(json.dumps(hashes, indent=2) + "\n", encoding="utf-8")
    return str(destination)


def main():
    parser = argparse.ArgumentParser(description="Build and test the Fluxboard OpenJK modules locally.")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--package", action="store_true")
    args = parser.parse_args()
    system = "windows" if os.name == "nt" else "linux"
    if platform.machine().lower() not in ("amd64", "x86_64"):
        parser.error("this delivery targets x86_64; use a matching x86_64 OpenJK engine")
    if args.sanitize and system == "windows":
        parser.error("the sanitizer profile is Linux-only")
    if args.sanitize and args.package:
        parser.error("sanitized modules are test artifacts, not release packages")
    build = (args.build_dir or ROOT / "build" / (system + ("-sanitize" if args.sanitize else ""))).resolve()
    build.mkdir(parents=True, exist_ok=True)
    report = {"status": "running", "system": system, "sanitized": args.sanitize, "engine_playtest": False}
    report_path = build / "flux-verification.json"
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    command = ["cmake", "-S", ROOT, "-B", build]
    command += ["-G", "Visual Studio 17 2022", "-A", "x64"] if system == "windows" else ["-G", "Ninja"]
    command += [f"-D{name}=OFF" for name in OFF]
    command += ["-DBuildMPGame=ON", "-DBuildMPCGame=ON", "-DBuildFluxTests=ON", "-DUseInternalLibs=ON",
                "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON",
                f"-DUseAddressSanitizer={'ON' if args.sanitize else 'OFF'}",
                f"-DUseUndefinedSanitizer={'ON' if args.sanitize else 'OFF'}"]
    try:
        run(command, build / "flux-configure.log")
        command = ["cmake", "--build", build, "--config", "Release", "--parallel", str(max(1, args.jobs)),
                   "--target", "jampgamex86_64", "cgamex86_64", "flux_tests", "flux_game_tests", "flux_client_tests"]
        if args.clean:
            command.append("--clean-first")
        run(command, build / "flux-build.log")
        run(["ctest", "--test-dir", build, "-C", "Release", "--output-on-failure", "--verbose"], build / "flux-tests.log")
        report["status"] = "passed"
        report["warning_diagnostics"] = 0
        report["scripted_simulation_ms"] = 300000
        report["source_commit"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
        if args.package:
            report["package"] = package(build, system)
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        report["status"] = "failed"
        report["error"] = str(error)
        print(str(error), file=sys.stderr)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    sys.exit(main())

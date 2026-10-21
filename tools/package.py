"""Package a Windows build with documentation and third-party notices."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--objdump", required=True, help="Native LLVM or MinGW objdump")
    args = parser.parse_args()
    executable = ROOT / "dist/FLUOstarExporter.exe"
    headers = subprocess.check_output([args.objdump, "-p", str(executable)], text=True)
    imports = re.findall(r"DLL Name:\s*(\S+)", headers)
    allowed = {"comctl32.dll", "ole32.dll", "shell32.dll", "msvcrt.dll", "gdi32.dll",
               "user32.dll", "kernel32.dll", "advapi32.dll"}
    unexpected = {name for name in imports if name.lower() not in allowed}
    if not imports or unexpected:
        raise RuntimeError(f"Unexpected runtime imports: {unexpected or 'no imports found'}")
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    dist = ROOT / "dist"
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    readme = readme.replace("[The build workflow](.github/workflows/build.yml)",
                            "The source repository’s `.github/workflows/build.yml` workflow")
    (dist / "README.txt").write_text(readme, encoding="utf-8")
    shutil.copy2(ROOT / "LICENSE", dist / "LICENSE")
    # The source report describes a particular validated artifact. Identify this build
    # separately without changing that historical report or claiming new UI validation.
    report = (ROOT / "VALIDATION.md").read_text(encoding="utf-8")
    report += f"\n## Packaged build\n\nSHA-256 of this package's executable: `{digest}`.\n"
    report += "This checksum identifies the packaged build; checks listed above describe the validated development build.\n"
    (dist / "VALIDATION.md").write_text(report, encoding="utf-8")
    files = [executable, dist / "README.txt", dist / "LICENSE", dist / "VALIDATION.md"]
    for directory, pattern in (("docs", "*.md"), ("licenses", "*.txt")):
        (dist / directory).mkdir(exist_ok=True)
        for source in sorted((ROOT / directory).glob(pattern)):
            destination = dist / directory / source.name
            shutil.copy2(source, destination)
            files.append(destination)
    checksums = dist / "SHA256SUMS.txt"
    checksums.write_text(digest + "  FLUOstarExporter.exe\n", encoding="ascii")
    files.append(checksums)
    archive = ROOT / "FLUOstarExporter-portable.zip"
    # Explicit file list excludes saved settings, measurements, and old build files.
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for file in files:
            package.write(file, "FLUOstarExporter/" + file.relative_to(dist).as_posix())
    with zipfile.ZipFile(archive) as package:
        if package.testzip() is not None:
            raise RuntimeError("Package checksum verification failed")
        assert hashlib.sha256(package.read("FLUOstarExporter/FLUOstarExporter.exe")).hexdigest() == digest
    print(f"Built {archive.name}; SHA-256 {digest}")


if __name__ == "__main__":
    main()

# FLUOstar Exporter

A portable Windows app that exports FLUOstar OMEGA measurements to Excel or CSV.

## Run from a USB stick

1. Extract `FLUOstarExporter-portable.zip` to a writable folder on the USB stick. If you built the app yourself, copy the contents of `dist` instead.
2. On the plate-reader PC, open `FLUOstarExporter.exe`.
3. Check **Data folder**. The app tries the saved path, `C:\Program Files (x86)\BMG`, the executable's folder and its parent, then `C:\Program Files\BMG`. It looks for `Measure.dbf` in the usual OMEGA subfolders. Use **Browse** if it does not find the database.
4. Choose **Export to**. The default is an `Exports` folder beside the executable.
5. Choose **Excel (.xlsx)** or **CSV (.csv)** under **Format**, select runs, and click **Export selected to XLSX** or **Export selected to CSV**.

Click a row to select it. **Shift-click** selects a range; **Ctrl-click** adds or removes individual rows. **Select all**, or **Ctrl+A** with the run list focused, selects all visible runs. **Clear** clears the selection. Search covers every displayed column, and clicking a column heading sorts the list. Only highlighted, visible runs are exported. **Refresh** reloads the database after new runs finish.

The app saves the format, database path, and output folder in `FLUOstarExporter.ini` beside the executable. The chosen format applies to all selected runs. The default `Exports` folder follows the executable if the USB drive letter changes; custom paths are saved as absolute paths.

Finish the plate-reader run before exporting. The app opens the database for reading only. Missing, incomplete, unsupported, or detectably changed measurement files fail individually, and the batch continues. **Stop** finishes the current run before stopping. Keep the USB stick connected until the app has closed. If you close the window during export, the app stops after the current run; close the window again once it finishes.

No installation, administrator access, Excel, Python, .NET, MARS, BDE, or database driver is required. The 32-bit executable targets 32-bit and 64-bit Windows 7, 8, 8.1, 10, and 11. UI checks have run in a Windows 7 CrossOver bottle on macOS; physical Windows hardware and USB operation remain unverified.

## Filenames

Files use the recorded measurement date and time:

```text
Example Protocol_20261021_143000.xlsx
```

CSV exports use the same name with a `.csv` extension. Protocol names come from `TSTNAME`, which may already be shortened in the database. Invalid Windows filename characters become underscores, trailing spaces and periods are removed, and long protocol names are shortened.

Existing exports are preserved. Duplicate names gain `_run<ID>`, then a number if needed. The final file appears after writing succeeds. An interrupted export can leave a `.partial` file or `.partial.lock` folder. After closing any exporter using that output folder, remove those unfinished entries before retrying, or choose another output folder.

## Excel and CSV contents

Each file starts with the available run metadata: run ID, protocol, measurement date/time, technique, plate, user, ID1–ID3, and protocol comment. Empty metadata fields are omitted. ID1–ID3 come from `IDNUMBER`, `IDNUMBER2`, and `IDNUMBER3`; the protocol comment comes from `KOMMENTAR`.

The measurement table adapts to the stored layout:

- Spectrum runs use wavelength/channel rows and well columns.
- Kinetic runs use time or measurement-point rows, with well or well/channel columns.
- Endpoint runs with one measurement field use channel rows and well columns.
- Sample IDs, or well-content labels when no sample ID is present, appear above the measurement columns.

Calibration references such as `0ABS` and `1ABS`, other technical records, and repeated database metadata are omitted from the measurement table. A stored timing record can supply kinetic row labels. Values are copied from the measurement table without recalculation. CSV preserves their stored decimal text. XLSX uses numeric cells where Excel can represent the value within its 15-significant-digit limit; other values remain text. Excel may display fewer decimal places than are stored.

XLSX files contain one `Measurements` sheet, with the rows through the data header frozen and filters on the measurement table. Excel is not needed to create them. CSV files use commas, quoted cells, CRLF line endings, a UTF-8 BOM, and decimal points. In Danish Excel, use **Data → From Text/CSV**, choose UTF-8, and select comma as the delimiter.

The app does not export MARS analysis definitions or results computed by analysis workflows, such as curve fits or workflow blank subtraction. It does not interpret memo fields. Unsupported layouts fail with an error. See [OMEGA database structure](docs/database-structure.md) for the supported layouts and field mappings.

## Validation

Earlier validation used a private database containing thousands of runs. All available measurement files exported to CSV; one indexed run with a missing file failed as expected. XLSX exports covering 23 distinct DBF layouts passed ZIP and XML checks and opened with openpyxl.

A reference spectrum with 381 wavelengths and 8 wells matched a MARS CSV within its three-decimal rounding precision (maximum absolute difference: 0.0005). This comparison validates that reference spectrum, rather than every measurement mode.

Regression checks cover parsing, dates, filenames, text encoding, compact layouts, duplicate measurements, file collisions, partial files, locks, deleted records, and missing or truncated tables. The Windows UI harness covers database discovery, selection, search, scrolling, both export formats, settings, and shutdown. See [VALIDATION.md](VALIDATION.md) for artifact details and the checks rerun for the current build.

## Build and development

The app uses C++17, native Win32 controls, and a built-in SpreadsheetML/ZIP writer. LLVM C++ runtimes are linked statically; MSVCRT is supplied by Windows. No vendor binaries are included. Runtime notices accompany the portable release in `licenses`.

All build scripts produce `dist/FLUOstarExporter.exe` and `FLUOstarExporter-portable.zip`. The ZIP includes the executable, README, MIT license, third-party runtime notices, technical documentation, validation notes, and SHA-256 checksum. Saved settings and database files are excluded. Building and packaging require Python 3; the finished app does not.

Use the [LLVM-MinGW 20241001 MSVCRT toolchain](https://github.com/mstorsjo/llvm-mingw/releases/tag/20241001). Each build produces a 32-bit Windows app for both 32-bit and 64-bit Windows. Linux and macOS are build hosts; the GUI runs on Windows.

### Windows

Extract `llvm-mingw-20241001-msvcrt-x86_64.zip` on a 64-bit Windows build PC (or the `i686` archive on a 32-bit PC). Install Python 3 with `python` available on PATH. From Command Prompt:

```bat
set LLVM_MINGW_MSVCRT=C:\tools\llvm-mingw-20241001-msvcrt-x86_64
tools\build.cmd
```

### Linux

On x86-64 Linux, extract `llvm-mingw-20241001-msvcrt-ubuntu-20.04-x86_64.tar.xz` and install Python 3. The archive includes the compiler, resource compiler, linker, and binary tools:

```sh
LLVM_MINGW_MSVCRT=/path/to/llvm-mingw-20241001-msvcrt-ubuntu-20.04-x86_64 \
bash tools/build.sh
```

### macOS

`tools/build.sh` uses Apple Clang, MinGW `windres`/`strip`/`objdump`, and LLVM's linker. Set `LLVM_MINGW_MSVCRT` to the extracted Windows toolchain. Use `LD_WINDOWS` for a native `ld.lld`, or run the Windows linker through CrossOver:

```sh
LLVM_MINGW_MSVCRT=/path/to/llvm-mingw-20241001-msvcrt-i686 \
CROSSOVER_WINE='/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine' \
CROSSOVER_BOTTLE=FluoExporterTest bash tools/build.sh
```

The LLVM i686 runtime requires LLVM's linker; GNU ld is not supported by this build. Packaging uses Python 3 and runs automatically after compilation on every supported build host.

### GitHub Actions

[The build workflow](.github/workflows/build.yml) runs on pushes, pull requests, and manual dispatch. It compiles and packages the Windows app on both Ubuntu 22.04 and Windows Server 2022, runs the core regression checks against a generated synthetic database, and uploads a portable ZIP from each build. Download the desired artifact from the workflow run's **Artifacts** section. Pushing a tag beginning with `v`, such as `v1.3.3`, also publishes a GitHub Release after both builds and their tests succeed. The release includes the Windows-built portable ZIP, a SHA-256 checksum for the ZIP, and generated release notes. The ZIP itself contains a checksum for the executable. Normal branch pushes, pull requests, and manual runs produce workflow artifacts only.

To publish a version, first commit and push your changes, then tag that commit:

```sh
git tag v1.3.3
git push origin v1.3.3
```

Use a new version tag for each release and update `src/app.rc` to match the release version before committing.

### Local regression checks

Run parser and export checks on macOS or Linux using a private or synthetic OMEGA database and fresh temporary output directories:

```sh
mkdir -p build
clang++ -std=c++17 -O2 tools/core_tests.cpp -o build/core-tests
build/core-tests /path/to/Omega/User/Data /tmp/fluo-core-tests-new
clang++ -std=c++17 -O2 tools/verify.cpp -o build/verify
build/verify /path/to/Omega/User/Data /tmp/fluo-full-export-new
```

For a test without instrument data, generate a synthetic fixture first:

```sh
python3 tools/make_test_fixture.py build/test-fixture
build/core-tests build/test-fixture build/test-output
```

Both fixture and output directories must be new. The full verifier reports indexed runs with missing files as failures. `tools/ui_smoke.cpp` is a Windows UI integration harness. Keep database fixtures and test exports outside version control.

## Publishing and privacy

Instrument installations, databases, exports, and used `FLUOstarExporter.ini` files can contain private measurements, names, comments, sample labels, and local paths. Do not commit them. Keep private data outside the repository. The `.gitignore` excludes local settings, compiled binaries, build output, and release archives. Check package contents before publishing compiled packages through GitHub Releases.

## License

The exporter source code and documentation are available under the [MIT License](LICENSE). Bundled third-party runtime components retain their own licenses; their notices are included in the portable package’s `licenses` folder and the GitHub source folder’s `licenses` directory.

# FLUOstar Exporter

A portable Windows app that exports FLUOstar OMEGA measurements to Excel or CSV.

## Run from a USB stick

1. Extract `FLUOstarExporter-portable.zip` to a writable folder on the USB stick. If you built the app yourself, copy the contents of `dist` instead.
2. On the plate-reader PC, open `FLUOstarExporter.exe`.
3. Check **Data folder**. The app tries the saved path, `C:\Program Files (x86)\BMG`, the executable's folder and its parent, then `C:\Program Files\BMG`. It looks for `Measure.dbf` in the usual OMEGA subfolders. Use **Browse** if it does not find the database.
4. Choose **Export to**. The default is an `Exports` folder beside the executable.
5. Choose **Excel (.xlsx)** or **CSV (.csv)** under **Format**, select runs, and click **Export selected to XLSX** or **Export selected to CSV**.

The **Measurement type** column shows **Kinetic assay** for protocols with multiple stored cycles or **Single point assay** for one cycle. This describes the cycles stored in each individual run. Script names and ID comments do not affect this classification. A script that repeats a single-cycle protocol still produces individual **Single point assay** entries; the exporter exports those entries separately. Missing or invalid cycle counts show **Measurement type unknown**. These labels are also searchable and included as separate measurement-type metadata in exports. **Technique** shows the measurement method.

Click a row to select it. **Shift-click** selects a range; **Ctrl-click** adds or removes individual rows. **Select all**, or **Ctrl+A** with the run list focused, selects all visible runs. **Clear** clears the selection. Search covers every displayed column, and clicking a column heading sorts the list. Only highlighted, visible runs are exported. The run list remains scrollable during and after export. The selected runs are captured when you click Export. **Refresh** reloads the database after new runs finish.

After a successful export, **Open export folder** opens the folder used by that export in Windows Explorer. It becomes available once at least one file has been exported and always opens the most recent successful export folder, even if you edit **Export to** afterward.

The app saves the format, database path, and output folder in `FLUOstarExporter.ini` beside the executable. The chosen format applies to all selected runs. The default `Exports` folder follows the executable if the USB drive letter changes; custom paths are saved as absolute paths.

Finish the plate-reader run before exporting. The app opens the database for reading only. Missing, incomplete, unsupported, or detectably changed measurement files fail individually, and the batch continues. **Stop** finishes the current run before stopping. Keep the USB stick connected until the app has closed. If you close the window during export, the app stops after the current run; close the window again once it finishes.

No installation, administrator access, Excel, Python, .NET, MARS, BDE, or database driver is required. The 32-bit executable targets 32-bit and 64-bit Windows 7, 8, 8.1, 10, and 11. UI checks have run in a Windows 7 CrossOver bottle on macOS; physical Windows hardware and USB operation remain unverified.

## Filenames

Files start with the recorded measurement date and time, followed by the protocol name (`YYYYMMDD_HHMMSS_ProtocolName`):

```text
20261021_143000_Example Protocol.xlsx
```

CSV exports use the same name with a `.csv` extension. Protocol names come from `TSTNAME`, which may already be shortened in the database. Invalid Windows filename characters become underscores, trailing spaces and periods are removed, and long protocol names are shortened.

Existing exports are preserved. Duplicate names gain `_1`, `_2`, and so on before the extension. The final file appears after writing succeeds. An interrupted export can leave a `.partial` file or `.partial.lock` folder. After closing any exporter using that output folder, remove those unfinished entries before retrying, or choose another output folder.

## Excel and CSV contents

Each file starts with the available run metadata: run ID, protocol, measurement date/time, technique, measurement type, optical settings, plate, user, ID1–ID3, and protocol comment. Empty metadata fields are omitted. ID1–ID3 come from `IDNUMBER`, `IDNUMBER2`, and `IDNUMBER3`; the protocol comment comes from `KOMMENTAR`.

Technique names follow the supplied BMG MARS definitions. In particular, database mode 3 is **Absorbance**, mode 4 is **Luminescence**, and mode 10 is **TRF**. Exports produced before this correction may have incorrect technique names and should be exported again.

The measurement table adapts to the stored layout:

- Spectrum runs use wavelength/channel rows and well columns.
- Kinetic runs use time or measurement-point rows, with well or well/channel columns. Kinetic spectra include a wavelength header and well/wavelength column labels.
- Endpoint runs use wavelength or channel rows and well columns. Absorbance wavelength selections are shown even when stored in protocol settings rather than a wavelength column.
- FI/TRF channels include the recorded excitation/emission filter settings; filter names are preserved as stored.
- Well scans use scan-field rows rather than a time axis.
- Sample IDs, or well-content labels when no sample ID is present, appear above the measurement columns.

Examples of the exported labels:

| Stored measurement | Export label or layout |
|---|---|
| Absorbance at 655 nm | `A01 (655 nm)` in a kinetic table; wavelength/channel rows for an endpoint table. |
| FI with filters `485-12` and `520` | `A01 (Ch 1; Ex 485-12; Em 520)` in a kinetic table; an **Optical settings** column for endpoint readings. |
| Kinetic absorbance spectrum | A **Wavelength (nm)** header identifies the columns, for example `A05 (380.0000 nm)`; time points remain as rows. |
| Well scan | **Scan field** rows such as `M1`, `M1_1`, and `M1_2`, preserving the stored field identities. |

Filter designations such as `485-12` remain text. The exporter does not interpret them as arithmetic, infer their bandwidth, or substitute a single wavelength. Genuine luminescence uses recorded emission settings; a channel number is not itself a wavelength. For techniques whose detector A/B mapping has not been verified, the export keeps the raw channel IDs and optical-setting metadata.

Calibration references such as `0ABS` and `1ABS`, other technical records, and repeated database metadata are omitted from the measurement table. A complete stored timing record can supply kinetic row labels. Channel IDs remain visible alongside optical labels; settings for techniques with unverified detector mappings remain in metadata. Values are copied from the measurement table without recalculation. CSV preserves their stored decimal text. XLSX uses numeric cells where Excel can represent the value within its 15-significant-digit limit; other values remain text. Excel may display fewer decimal places than are stored.

XLSX files contain one `Measurements` sheet, with the rows through the data header frozen and filters on the measurement table. Excel is not needed to create them. CSV files use commas, quoted cells, CRLF line endings, a UTF-8 BOM, and decimal points. In Danish Excel, use **Data → From Text/CSV**, choose UTF-8, and select comma as the delimiter.

The app does not export MARS analysis definitions or results computed by analysis workflows, such as curve fits or workflow blank subtraction. It does not interpret memo fields. Unsupported layouts fail with an error. See [OMEGA database structure](docs/database-structure.md) for the supported layouts and field mappings.

## Validation

The current audit checked 3,047 available measurement files across 23 DBF layouts. All 2,362,780 source measurement values matched their compact export coordinates exactly. All available files exported to CSV; one indexed run with a missing file failed as expected. XLSX exports covering all 23 layouts passed ZIP/XML checks, and every nonempty worksheet cell matched its corresponding CSV text.

A reference spectrum with 381 wavelengths and 8 wells matched a MARS CSV within its three-decimal rounding precision (maximum absolute difference: 0.0005). This comparison validates that reference spectrum, rather than every measurement mode.

Regression checks cover parsing, dates, filenames, text encoding, compact layouts, duplicate measurements, file collisions, partial files, locks, deleted records, and missing or truncated tables. The Windows UI harness covers database discovery, selection, search, scrolling, both export formats, settings, and shutdown. See [VALIDATION.md](VALIDATION.md) for artifact details and [Export layout audit](docs/export-audit.md) for the technique, filter, wavelength, and scan-axis checks.

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

[The build workflow](.github/workflows/build.yml) runs on pushes, pull requests, and manual dispatch. It compiles and packages the Windows app on both Ubuntu 22.04 and Windows Server 2022, runs the core regression checks against a generated synthetic database, and uploads a portable ZIP from each build. Download the desired artifact from the workflow run's **Artifacts** section. Pushing a tag beginning with `v`, such as `v1.3.1`, also publishes a GitHub Release after both builds and their tests succeed. The release includes the Windows-built portable ZIP, a SHA-256 checksum for the ZIP, and generated release notes. The ZIP itself contains a checksum for the executable. Normal branch pushes, pull requests, and manual runs produce workflow artifacts only.

To publish a version, first commit and push your changes, then tag that commit:

```sh
git tag v1.3.1
git push origin v1.3.1
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

To audit every available file against its compact output without writing exports:

```sh
clang++ -std=c++17 -O2 -Wall -Wextra -Werror tools/audit.cpp -o build/audit
build/audit /path/to/Omega/User/Data
```

The audit counts missing files separately and returns a failure for parsing or value/coordinate mismatches. It does not validate MARS analysis results or establish the physical meaning of unverified layouts.

Both fixture and output directories must be new. The full verifier reports indexed runs with missing files as failures. `tools/ui_smoke.cpp` is a Windows UI integration harness.

## License

The exporter source code and documentation are available under the [MIT License](LICENSE). Bundled third-party runtime components retain their own licenses; their notices are included in the portable package’s `licenses` folder and the GitHub source folder’s `licenses` directory.

# Validation — 2026-10-21

Artifact: v1.3.3 `dist/FLUOstarExporter.exe`, 32-bit Windows GUI executable, subsystem 6.1.

SHA-256: `2c0922a2a9ce522d17409d5e9bda87a189c70a99c575fc2180eeadb1a3b27789`

## Checks rerun for v1.3.3

- The Windows app compiled with `-Wall -Wextra -Werror` using Apple Clang, LLVM-MinGW 20241001 MSVCRT headers/libraries, and LLVM's linker through CrossOver.
- Imported DLLs: COMCTL32, ole32, SHELL32, msvcrt, GDI32, USER32, and KERNEL32. No separately shipped runtime is required.
- Core regression checks passed, including endpoint label alignment, duplicate-cell rejection when the first value is blank, DBF date fields, leap years, invalid calendar dates, filenames, Windows-1252 decoding, CSV quoting, measurement-field discovery, collisions, partial-file preservation, locks, deleted records, and missing/truncated tables.
- The private catalogue contained 3,048 runs. All 3,047 available measurement files exported to CSV, containing 2,256,882 plate-well source records in total. The one missing measurement file produced the expected failure. These counts describe source records, not output rows or individual measurement cells.
- XLSX exports covering all 23 distinct DBF layouts passed ZIP checksums and XML parsing. openpyxl read every worksheet cell in those workbooks.
- The reference spectrum comparison passed: 381 wavelengths × 8 wells, maximum absolute difference 0.0005 against the MARS CSV. The exporter retains the stored decimal values; MARS's reference file uses three decimals.

- The v1.3.3 executable passed the Windows UI harness in the dedicated Windows 7 CrossOver bottle: automatic discovery, loading, hover scrolling, Shift/Ctrl selection, Clear, status and run-comment search, Select all, XLSX/CSV exports, saved source/format after restart, and clean exit.

## Fixes in this build

- Endpoint well and content labels align with their measurement columns.
- Duplicate cells fail even when the earlier measurement is blank.
- Date validation checks month lengths and leap years and accepts the DBF `YYYYMMDD` date form.
- Search includes the Status column. Progress says “Processed” so failed runs are not described as exported.
- Documentation explains metadata omissions, Excel numeric precision, saved paths, interrupted exports, and the limits of consistency checks.

## Limits

Physical Windows hardware, the plate-reader PC, USB removal behavior, and exports while the instrument is writing remain unverified. Export completed runs. File size and modification-time checks can detect changes during a read, but do not provide a consistent database snapshot.

Only the reference spectrum has been compared cell by cell with a MARS export. Successful parsing and workbook validation across the other layouts do not independently establish agreement with every vendor measurement or analysis mode.

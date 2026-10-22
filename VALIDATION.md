# Validation — 2026-10-22

## Current build

The current local build includes the separate **Measurement type** column, corrected BMG technique-code mapping, optical labels for endpoint and kinetic exports, wavelength headers for kinetic spectra, and spatial scan-field labels. Script comments are not used to classify or group measurements. Version resources and the application manifest are set to 1.3.1. Measurement types are displayed and exported as **Single point assay** and **Kinetic assay**.

Executable: `dist/FLUOstarExporter.exe`, 32-bit Windows GUI, subsystem 6.1.

SHA-256: `5b392f6cde05c76d9dfdeefc1402f98c1e4997656c46e4e5d8314bfdc4cbed97`.

## Checks completed

| Check | Result |
|---|---|
| Technique-code mapping | All 12 recognized codes agree with `tpMessTypID` in the supplied BMG MARS `Config/Omega/DbMap.xml`. |
| Source value/coordinate audit | 3,047 available files, 23 DBF layouts, and 2,362,780 measurement values checked; zero mismatches. |
| CSV batch export | All 3,047 available files exported, containing 2,256,882 plate-well source records. The missing file for run 3103 failed as expected. |
| XLSX layout coverage | All 23 layouts passed ZIP checksum and XML parsing checks; 42,669 nonempty worksheet cells matched the corresponding CSV text exactly. |
| Native core regression checks | Passed with private instrument data. |
| Windows core regression checks | Passed with synthetic fixtures in a 32-bit Windows executable under CrossOver. |
| Windows UI harness | Passed discovery, selection, search, hover scrolling before/during/after exports, both formats, export-folder opening, saved settings, and shutdown. |
| Windows/native output comparison | The UI harness's CSV and XLSX exports matched the corresponding native artifacts byte for byte. |
| Build and packaging | Built with `-Wall -Wextra -Werror`; portable ZIP contents, runtime imports, and executable checksum verified. |

The value audit counts individual stored measurement cells, including blanks. The CSV record count counts source plate-well records; a record can contain multiple measurement fields. These totals do not count the same thing.

The new regression cases cover fluorescence filter pairing, absorbance wavelength mapping, emission-only interpretation for luminescence, kinetic spectra, spatial scans, incomplete time labels, missing wavelengths, conflicting labels, and duplicate measurement coordinates. Existing checks cover dates, leap years, filenames, duplicate export names, Windows-1252 text, CSV quoting, endpoint label alignment, partial files, locks, deleted records, and missing/truncated DBFs.

The build uses Apple Clang and the LLVM-MinGW 20241001 MSVCRT toolchain, with LLVM's linker through CrossOver. The portable app uses Windows system DLLs and requires no separately shipped runtime. CrossOver checks ran in the dedicated Windows 7 test bottle on macOS.

See [Export layout audit](docs/export-audit.md) for specific findings, examples, the audit command, and interpretation limits. See [Database structure](docs/database-structure.md) for field mappings and export layouts.

## Earlier reference comparison

A reference absorbance spectrum with 381 wavelengths and 8 wells matched a MARS CSV within its three-decimal rounding precision: maximum absolute difference 0.0005. This establishes agreement for that reference spectrum. The current all-file audit establishes preservation of stored source values; it is not an independent comparison with MARS for every measurement mode.

Earlier development reports used the label v1.3.3 and executable checksum `2c0922a2a9ce522d17409d5e9bda87a189c70a99c575fc2180eeadb1a3b27789`. That checksum identifies the historical artifact, not the current executable above. Packaged builds also contain their own executable checksum in `SHA256SUMS.txt`.

## Limits

The inspected private data contains FI, multi-channel FI, absorbance, TRF, and absorbance spectra. Genuine luminescence is covered by synthetic regression cases only. Techniques absent from this data are not established by the private-data audit, and detector A/B associations are not inferred. Filter names remain literal stored designations. Spatial scan positions are not reconstructed.

Physical Windows hardware, the plate-reader PC, USB removal behavior, and exports during acquisition remain unverified. Export completed runs. File-size and modification-time checks can detect some changes during reading, but do not provide a consistent database snapshot.

The exporter does not evaluate MARS analysis workflows, curve fits, or blank subtraction, and these checks do not validate those results or physical acquisition timing.

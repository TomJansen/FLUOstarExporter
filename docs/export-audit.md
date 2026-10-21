# Export layout audit — 2026-10-21

The audit used the supplied private OMEGA database and the BMG MARS `Config/Omega/DbMap.xml` bundled with it. Vendor software and database files are not included in the portable release.

## Findings and fixes

- The old technique map misidentified mode 3 as luminescence, mode 1 as absorbance, and several other codes. All 12 recognized codes now agree with the vendor's `tpMessTypID` lookup. The private catalogue contains FI, multi-channel FI, absorbance, TRF, and absorbance spectra; it contains no actual luminescence records.
- FI/TRF optical settings live in tab-separated `EXFILT` and `EMFILT` catalogue fields. Exports now retain the original filter designations, including suffixes such as `485-12`, alongside channel identifiers. They are not relabelled as spectrum wavelengths.
- Absorbance selections in `EXFILT` now label endpoint and kinetic exports with wavelengths when the numbered DBF lacks `WAVELENGTH`. For example, run 2102 is absorbance at 655 nm, not luminescence. Run 1824 is FI with excitation filter `485-12` and emission filter `520`.
- Kinetic spectra retain their explicit wavelength headers (run 11: 380–800 nm, 211 wavelengths and 32 stored time points for A05). The stored protocol cycle count is not assumed to equal the number of measurement fields.
- Run 12 is a well scan (`MODE = 50`), not a 717-point kinetic run. Its timing record contains one time and otherwise blank fields. The export now uses `Scan field` and preserves `M1`, `M1_1`, …, `M1_716` as field identities. Spatial positions are not reconstructed.
- Incomplete time labels fall back to field identities. Multiple timing records, duplicate multi-point coordinates, missing wavelengths, and conflicting well labels are rejected.

## Verification

`tools/audit.cpp` checked 3,047 available files across 23 DBF layouts. All 2,362,780 source measurement values matched their compact output coordinates exactly, including blank values and stored decimal text. The one missing catalogue entry, run 3103, was reported separately. The initial scan found no duplicate source coordinates, conflicting well labels, or blank plate-well wavelengths in the available database.

All 3,047 available files exported successfully to CSV. XLSX exports covering all 23 layouts passed ZIP checksums and XML parsing. Their 42,669 nonempty worksheet cells matched the corresponding CSV text exactly. Synthetic regression checks cover fluorescence filter pairing, multi-channel settings, absorbance wavelength mapping, luminescence emission-only interpretation, spectral kinetics, spatial scan fields, incomplete timings, and malformed coordinates/labels.

The Windows app built with `-Wall -Wextra -Werror`. The new regression cases also passed in a 32-bit Windows test executable under CrossOver. The Windows UI harness passed source discovery, selection, search, scrolling, both export formats, export-folder opening, settings persistence, and shutdown. Its CSV/XLSX output matched the validated native artifacts byte for byte.

These checks validate preservation and the observed layouts. Luminescence interpretation has synthetic coverage only. Techniques absent from the private data are not validated by this database audit. Multi-detector channel A/B associations are not inferred; raw channel IDs and stored optical-setting metadata remain available. Stored filter names are not expanded into bandwidths or guessed wavelengths. The audit does not validate MARS analysis results, physical timing, or instrument operation.

## Repeat the value audit

```sh
mkdir -p build
clang++ -std=c++17 -O2 -Wall -Wextra -Werror tools/audit.cpp -o build/audit
build/audit /path/to/Omega/User/Data
```

The audit reads the instrument database without writing exports or changing its files. Missing files are counted separately from mapping failures.

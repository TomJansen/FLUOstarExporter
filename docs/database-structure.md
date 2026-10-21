# OMEGA database structure

This document describes the database layout observed in the FLUOstar OMEGA database used to develop this exporter. It is based on DBF headers and behavior observed during development; it is not an official BMG LABTECH specification. Field presence and meaning can vary with instrument firmware, software version, measurement mode, and protocol.

## Directory layout

The exporter searches for a directory containing `Measure.dbf`, normally:

```text
BMG/
└── Omega/
    └── User/
        └── Data/
            ├── Measure.dbf
            ├── Measure.DBT
            ├── 1.dbf
            ├── 2.dbf
            ├── ...
            └── <run-id>.dbf
```

`Measure.dbf` is the run catalogue. Each active catalogue record describes one plate-reader run. Its numeric `MESSDATEI` value identifies the corresponding numbered measurement table:

```text
Measure.dbf record
    MESSDATEI = <run-id>
          │
          └──────────────► <run-id>.dbf
```

`Measure.DBT` is the dBASE memo sidecar used by memo fields in the catalogue. The exporter reads catalogue fields stored directly in `Measure.dbf`; it does not interpret MARS analysis objects stored in memo blocks.

An indexed run can occasionally have no matching numbered DBF. The UI lists it as **Data file missing**, and exporting that run fails without stopping the rest of a batch.

## File relationship

```mermaid
erDiagram
    MEASURE_CATALOGUE ||--o| RUN_TABLE : "MESSDATEI -> filename"
    MEASURE_CATALOGUE {
        numeric MESSDATEI PK
        character TSTNAME
        character DATE
        character TIME
        numeric MESSTYP
        character MPNAME
        character USER
        character IDNUMBER
        character IDNUMBER2
        character IDNUMBER3
        character KOMMENTAR
    }
    RUN_TABLE {
        character WELLNUM
        character CONTENT
        character SAMPLEID
        character CH
        numeric WAVELENGTH
        numeric PATHLENGTH
        numeric M1
        numeric M2_to_Mn
        numeric TIME
        numeric TEMP
    }
```

The relationship is logical rather than a declared database constraint. DBF files do not provide foreign keys.

## DBF representation

The files use the dBASE record model:

- A 32-byte file header contains the record count, header length, record length, and language-driver byte. It is followed by one 32-byte descriptor per field and a header terminator.
- Records are fixed-width byte sequences. Numeric values are stored as right-aligned text, not binary floating-point numbers.
- The first byte of every record is a deletion marker. A space means active; `*` means deleted. The exporter skips deleted records.
- Character fields in the supported installations use a Windows Western encoding compatible with Windows-1252. The exporter converts them to UTF-8.
- Supported field types are `C` (character), `N`/`F` (numeric), `L` (logical), `D` (date), and `M` (memo pointer). Numbered measurement tables containing memo fields are rejected.

The reader validates record sizes and field descriptors before parsing. It also checks the file size and modification time again after reading, which detects changes that alter either attribute. These checks do not provide a database snapshot or guarantee consistency while the instrument is writing.

## `Measure.dbf`: run catalogue

The catalogue has many protocol and instrument-setting fields. Only a small stable subset is required to locate and describe exports.

| Field | Role in the exporter |
|---|---|
| `MESSDATEI` | Numeric run identifier and numbered DBF filename stem. |
| `TSTNAME` | Protocol name; used in the output filename. |
| `DATE`, `TIME` | Recorded run date and time. Dates accept `YYYY-MM-DD`, `YYYY/MM/DD`, `YYYY.MM.DD`, or the DBF date form `YYYYMMDD`; times use `HH:MM:SS`. Invalid calendar dates are rejected. |
| `MESSTYP` | Numeric measurement-technique code; mapped using the bundled BMG MARS definitions below. |
| `KINETICS`, `KINETICS2`–`KINETICS4` | Stored cycle counts. The exporter totals nonempty numeric fields to distinguish single from multiple cycles; missing, zero-total, or invalid counts show an unknown measurement type. |
| `MODE` | Acquisition layout; `50` identifies a spatial well scan. |
| `EXFILT`, `EMFILT` | Tab-separated optical settings. These supply filter labels and, for absorbance, wavelength selections. |
| `MPNAME` | Plate definition/name. |
| `USER` | Stored instrument user. |
| `IDNUMBER` | Run comment shown as ID1. |
| `IDNUMBER2` | Run comment shown as ID2. |
| `IDNUMBER3` | Run comment shown as ID3. |
| `KOMMENTAR` | Protocol comment. |

The `MESSTYP` mapping below follows `tpMessTypID` in the supplied BMG MARS `Config/Omega/DbMap.xml`, whose `SP_MeasTyp` rule reads `MESSTYP` directly. The earlier exporter mapping incorrectly called mode 3 luminescence and mode 1 absorbance.

| Value | Displayed technique |
|---:|---|
| 0 | FI |
| 1 | FI (multi) |
| 2 | FP |
| 3 | Absorbance |
| 4 | Luminescence |
| 5 | Nephelometry |
| 6 | TRF (2 channels) |
| 7 | Dual luminescence |
| 8 | Dual FI |
| 9 | AlphaScreen |
| 10 | TRF |
| 11 | Absorbance spectrum |

Unmapped values remain visible as `Mode <value>`; the vendor maps code 12 to undefined.

`EXFILT` and `EMFILT` contain tab-separated optical settings. The vendor map identifies these as excitation/emission settings for fluorescence and as wavelength selections for absorbance. The exporter retains filter names such as `485-12` literally; it does not interpret the suffix or convert a filter designation into a single wavelength. For absorbance (mode 3), numeric `EXFILT` selections supply wavelengths when a measurement table has no `WAVELENGTH` field. Channel numbers select entries in the stored order. A single stored setting applies to each chromatic selection. Luminescence (mode 4) uses emission settings without interpreting `EXFILT` as excitation. Multi-detector techniques retain their raw channel IDs and optical-setting metadata; detector A/B mappings are not inferred.

The separate **Measurement type** column uses the stored `KINETICS` fields to distinguish single measurements from kinetic protocols. It does not use script labels or combine separate runs. This cycle-count classification does not assert that the count equals the number of stored measurement fields; run 11 records `KINETICS = 8` but has 32 stored time points. `MODE = 50` identifies a well scan: its multiple measurement fields are spatial scan values, not kinetic times.

Other catalogue groups include gain settings, kinetic timing, shaking, injection/pump configuration, plate geometry, temperature targets, reader identity, firmware version, calibration values, and memo references. The exporter does not copy those settings wholesale into the compact export.

## Numbered DBFs: measurement tables

Numbered tables vary by measurement mode. The exporter discovers columns from each DBF header instead of assuming one fixed schema.

| Field or pattern | Observed role |
|---|---|
| `WELLNUM` | Plate-well identifier such as `A01`; non-well tokens can identify calibration or technical rows. |
| `CONTENT` | Short content/well label. |
| `SAMPLEID` | Optional, longer sample identifier. It takes precedence over `CONTENT` in the compact header. |
| `CH` | Channel or series identifier. Special technical records can use nonnumeric values. |
| `WAVELENGTH` | Optional wavelength axis for spectrum runs. |
| `PATHLENGTH` | Optional stored path-length value. |
| `M1`, `M2`, … | Measurement values or points, in physical DBF field order. |
| `M1_1`, `M1_2`, … | Continuation naming observed in well scanning; not evidence of kinetics on its own. |
| `TIME` | Stored time value; used as the row axis when repeated well/channel records indicate kinetics. |
| `TEMP` | Stored temperature value. |
| `KONZ_CHAR`, `KONZ_MANT` | Stored concentration components used by the vendor software. |
| `DILUTION` | Stored dilution value. |
| `VOLPUMP1`–`VOLPUMP4` | Stored pump-volume fields. |

Names beginning with `M` are treated as measurement fields only when they match `M<digits>` or `M<digits>_<digits>`. The physical field order defines their sequence.

## Plate rows and technical rows

A plate-well row has one or more uppercase ASCII letters followed by digits, for example `A01` or `AA01`. Rows whose `WELLNUM` does not match that form are treated as internal records. Observed internal rows include absorbance references such as `0ABS` and `1ABS`, timing rows, and temperature/helper rows.

The compact exporter keeps plate-well measurements and omits internal rows. This is the main difference between the compact output and a raw DBF dump.

## How tables become compact exports

The output begins with selected catalogue metadata. The numbered DBF is then pivoted according to its available axes:

| Detected layout | Export layout |
|---|---|
| `WAVELENGTH` plus one measurement field | One row per wavelength/channel; one column per well. |
| One measurement field with repeated well/channel pairs | One row per stored time/channel; one column per well. Duplicate cells are rejected, including duplicates whose first value is blank. |
| One measurement field without repeated pairs | One row per channel or mapped absorbance wavelength; one column per well. FI/TRF optical settings accompany the channel. |
| Multiple measurement fields | One row per measurement point; one column per well/channel. A single complete technical `CH = t` row supplies the time labels. Explicit or mapped absorbance wavelengths label the columns; FI/TRF filter labels accompany channel identities. |

Well scans use `Scan field` rows containing the stored measurement field names. Spatial coordinates are not reconstructed. Incomplete kinetic timing rows fall back to measurement field identities. Duplicate timing records, conflicting well labels, missing wavelengths, and duplicate measurement coordinates are rejected rather than silently relabelled or overwritten.

The measurement values are copied from the numbered DBF without recalculation. The exporter does not evaluate MARS curve fits, blank-subtraction workflows, memo objects, or other analysis definitions.

## Read-only and consistency behavior

The exporter never writes to the BMG directory. It opens each DBF for binary input, parses active records, and writes the result to the chosen output directory. The UI rejects the database folder itself as an output folder; choose a separate export location. If a run table is missing, truncated, unsupported, or changes while being read, that run fails with an error and the remaining selected runs continue.


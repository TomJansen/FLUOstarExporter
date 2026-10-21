"""Compare a compact spectrum CSV with a slash-delimited MARS spectrum export."""
import csv
from decimal import Decimal
from pathlib import Path
import sys


if len(sys.argv) != 3:
    raise SystemExit("Usage: compare_exports.py COMPACT.csv MARS.csv")

with open(sys.argv[1], encoding="utf-8-sig", newline="") as stream:
    compact = list(csv.reader(stream))

header_index = next(
    (index for index, row in enumerate(compact) if row and row[0] == "Wavelength (nm)"),
    None,
)
if header_index is None:
    raise SystemExit("Compact export has no wavelength table")

compact_values = {}
for row in compact[header_index + 1 :]:
    if not row or not row[0]:
        continue
    # Column 2 is the first well because column 1 contains the channel.
    compact_values[Decimal(row[0])] = [Decimal(value) for value in row[2:] if value]

mars_lines = Path(sys.argv[2]).read_text(encoding="cp1252").splitlines()
mars_values = {}
for line in mars_lines:
    fields = line.split("/")
    if len(fields) >= 3 and fields[0].startswith("Spectrum Lambda"):
        mars_values[Decimal(fields[1])] = [Decimal(value) for value in fields[2:] if value]

if compact_values.keys() != mars_values.keys():
    raise SystemExit("Wavelength rows differ")

differences = []
for wavelength, expected in mars_values.items():
    actual = compact_values[wavelength]
    if len(actual) != len(expected):
        raise SystemExit(f"Well count differs at {wavelength} nm")
    differences.extend(abs(left - right) for left, right in zip(actual, expected))

maximum = max(differences, default=Decimal(0))
if maximum > Decimal("0.0005"):
    raise SystemExit(f"Measurements differ; maximum absolute difference is {maximum}")
print(
    f"PASS: {len(mars_values)} wavelengths, {len(differences)} cells; "
    f"maximum absolute difference {maximum}"
)

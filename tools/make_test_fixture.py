"""Create a small synthetic OMEGA database for public CI tests."""
from pathlib import Path
import struct
import sys


def dbf(path, fields, rows):
    header_size = 32 + len(fields) * 32 + 1
    record_size = 1 + sum(size for _, _, size in fields)
    header = bytearray(32)
    header[0] = 3
    header[29] = 3
    struct.pack_into("<IHH", header, 4, len(rows), header_size, record_size)
    with path.open("wb") as stream:
        stream.write(header)
        for name, kind, size in fields:
            descriptor = bytearray(32)
            encoded = name.encode("ascii")
            descriptor[:len(encoded)] = encoded
            descriptor[11] = ord(kind)
            descriptor[16] = size
            stream.write(descriptor)
        stream.write(b"\r")
        for row in rows:
            stream.write(b" ")
            for value, (_, kind, size) in zip(row, fields):
                encoded = value.encode("cp1252")
                if len(encoded) > size:
                    raise ValueError("Fixture field overflow")
                stream.write(encoded.rjust(size) if kind == "N" else encoded.ljust(size))
        stream.write(b"\x1a")


def main():
    if len(sys.argv) != 2:
        raise SystemExit("Usage: make_test_fixture.py NEW_FOLDER")
    folder = Path(sys.argv[1])
    folder.mkdir(parents=True, exist_ok=False)
    dbf(folder / "Measure.dbf",
        [("MESSDATEI", "N", 6), ("TSTNAME", "C", 32), ("DATE", "C", 10),
         ("TIME", "C", 8), ("MESSTYP", "N", 2), ("IDNUMBER", "C", 16)],
        [["1", "Synthetic test", "2024-02-29", "12:34:56", "1", "æøå"]])
    dbf(folder / "1.dbf",
        [("WELLNUM", "C", 4), ("CH", "C", 1), ("M1", "N", 12), ("CONTENT", "C", 16)],
        [["A01", "1", "1.2500", "Sample A"], ["A02", "1", "2.5000", "Sample B"],
         ["0ABS", "1", "0.0000", "Reference"]])


if __name__ == "__main__":
    main()

#include "../src/core.hpp"
#include <iostream>
using namespace fluo;
static void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
static void fails(const std::function<void()> &fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::exception &) {
        caught = true;
    }
    require(caught, "Expected failure");
}
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Usage: core_tests SOURCE TEMP_OUTPUT");
        auto runs = loadRuns(argv[1]);
        require(!runs.empty(), "No runs loaded");
        auto it = std::find_if(runs.begin(), runs.end(), [](auto &r) { return r.available; });
        require(it != runs.end(), "No available fixture run");
        auto run = *it;
        run.protocol = "Example Protocol";
        run.stamp = "2026-10-21 14:30:00";
        auto original = readDbf(run.data);
        fs::path out = argv[2];
        require(!fs::exists(out), "Use a new temporary test folder");
        fs::create_directories(out);
        auto a = exportRun(run, out), b = exportRun(run, out), c = exportRun(run, out);
        require(a.path.filename() == "20261021_143000_Example Protocol.csv",
                "CSV filename must start with measurement date and time");
        require(b.path.filename() == "20261021_143000_Example Protocol_1.csv" &&
                    c.path.filename() == "20261021_143000_Example Protocol_2.csv",
                "Duplicate CSV filenames must use sequential suffixes");
        require(a.records > 0 && b.records == a.records, "Compact measurement count");
        require(timestamp("2024/01/23", "12:34:56") == "2024-01-23 12:34:56", "Date format");
        fails([] { timestamp("23/01/2024", "12:34:56"); });
        fails([] { timestamp("2024/13/23", "12:34:56"); });
        require(timestamp("20240229", "00:00:00") == "2024-02-29 00:00:00",
                "DBF date field and leap day");
        require(timestamp("2000-02-29", "23:59:59") == "2000-02-29 23:59:59",
                "Leap century");
        fails([] { timestamp("2024-02-31", "12:34:56"); });
        fails([] { timestamp("2023-02-29", "12:34:56"); });
        fails([] { timestamp("1900-02-29", "12:34:56"); });
        fails([] { timestamp("2024-04-31", "12:34:56"); });
        fails([] { timestamp("0000-01-01", "12:34:56"); });
        fails([] { timestamp("2024-01-01", "24:00:00"); });
        Table endpoint;
        endpoint.fields = {{"WELLNUM", 'C', 3}, {"CH", 'C', 1}, {"M1", 'N', 8},
                           {"CONTENT", 'C', 8}};
        endpoint.rows = {{"A01", "1", "1.25", "Sample A"},
                         {"A02", "1", "2.50", "Sample B"}};
        auto compact = compactExport(run, endpoint);
        auto header = compact.dataHeaderRow - 1;
        require(compact.rows[header - 2] == Row({"Well", "A01", "A02"}) &&
                    compact.rows[header - 1] == Row({"Content", "Sample A", "Sample B"}) &&
                    compact.rows[header] == Row({"Channel", "A01", "A02"}) &&
                    compact.rows[header + 1] == Row({"1", "1.25", "2.50"}),
                "Endpoint labels must align with their measurement columns");
        Table kineticSpectrum;
        kineticSpectrum.fields = {{"WELLNUM", 'C', 4}, {"CH", 'C', 3},
                                  {"WAVELENGTH", 'N', 12}, {"M1", 'N', 12}, {"M2", 'N', 12}};
        kineticSpectrum.rows = {{"", "t", "", "0.0000", "4.0000"},
                                {"A01", "1", "380.0000", "0.1200", "0.1300"},
                                {"A01", "2", "382.0000", "0.2200", "0.2300"},
                                {"A02", "1", "380.0000", "0.3200", "0.3300"},
                                {"A02", "2", "382.0000", "0.4200", "0.4300"}};
        auto spectrum = compactExport(run, kineticSpectrum);
        auto spectrumHeader = spectrum.dataHeaderRow - 1;
        require(spectrum.rows[spectrumHeader - 1] ==
                    Row({"Wavelength (nm)", "380.0000", "382.0000", "380.0000", "382.0000"}) &&
                    spectrum.rows[spectrumHeader] ==
                    Row({"Time", "A01 (380.0000 nm)", "A01 (382.0000 nm)",
                         "A02 (380.0000 nm)", "A02 (382.0000 nm)"}) &&
                    spectrum.rows[spectrumHeader + 1] ==
                    Row({"0.0000", "0.1200", "0.2200", "0.3200", "0.4200"}) &&
                    spectrum.rows[spectrumHeader + 2] ==
                    Row({"4.0000", "0.1300", "0.2300", "0.3300", "0.4300"}),
                "Kinetic spectrum must label each well/wavelength column and retain time/value alignment");
        // Filters live in catalogue fields, not necessarily in WAVELENGTH.
        auto opticalRun = run;
        opticalRun.meta = {{"MESSTYP", "0"}, {"EXFILT", "485-12\t"}, {"EMFILT", "520\t"}};
        auto fi = compactExport(opticalRun, endpoint);
        require(fi.rows[fi.dataHeaderRow - 1] == Row({"Channel", "Optical settings", "A01", "A02"}) &&
                    fi.rows[fi.dataHeaderRow] == Row({"1", "Ex 485-12; Em 520", "1.25", "2.50"}),
                "FI endpoint must show its filter designations without changing values");
        opticalRun.meta = {{"MESSTYP", "3"}, {"EXFILT", "450\t595\t"}, {"EMFILT", "Lens"}};
        auto absorbance = compactExport(opticalRun, endpoint);
        require(absorbance.rows[absorbance.dataHeaderRow] == Row({"450", "1", "1.25", "2.50"}),
                "Absorbance must map channel to stored wavelength");
        opticalRun.meta = {{"MESSTYP", "1"}, {"EXFILT", "485-12\t584\t"}, {"EMFILT", "520\t615\t"}};
        auto optics = channelOptics(opticalRun, "2");
        require(optics.excitation == "584" && optics.emission == "615" && optics.wavelength.empty(),
                "FI multi-channel filter settings must follow channel order");
        opticalRun.meta["MESSTYP"] = "4";
        optics = channelOptics(opticalRun, "2");
        require(optics.excitation.empty() && optics.emission == "615",
                "Luminescence must not mislabel a stored EXFILT value as excitation");
        require(channelOptics(opticalRun, "t").description().empty(), "Technical channel has no optics");
        Table fiKinetic;
        fiKinetic.fields = {{"WELLNUM", 'C', 4}, {"CH", 'C', 2}, {"M1", 'N', 10}, {"M2", 'N', 10}};
        fiKinetic.rows = {{"", "t", "0", "4"}, {"A01", "1", "1.2500", "1.5000"}};
        opticalRun.meta = {{"MESSTYP", "0"}, {"EXFILT", "485-12\t"}, {"EMFILT", "520\t"}};
        auto kinetic = compactExport(opticalRun, fiKinetic);
        require(kinetic.rows[kinetic.dataHeaderRow - 1][1] == "A01 (Ch 1; Ex 485-12; Em 520)" &&
                    kinetic.rows[kinetic.dataHeaderRow] == Row({"0", "1.2500"}),
                "FI kinetics must retain filter labels, time and decimal values");
        opticalRun.meta = {{"MESSTYP", "3"}, {"EXFILT", "595\t"}};
        kinetic = compactExport(opticalRun, fiKinetic);
        require(kinetic.rows[kinetic.dataHeaderRow - 1][1] == "A01 (595 nm)",
                "Absorbance kinetics must label catalogue wavelengths");
        opticalRun.meta = {{"MODE", "50"}, {"MESSTYP", "0"}};
        fiKinetic.fields[3].name = "M1_1";
        fiKinetic.rows[0][3].clear();
        auto scan = compactExport(opticalRun, fiKinetic);
        require(scan.rows[scan.dataHeaderRow - 1][0] == "Scan field" &&
                    scan.rows[scan.dataHeaderRow] == Row({"M1", "1.2500"}) &&
                    scan.rows[scan.dataHeaderRow + 1] == Row({"M1_1", "1.5000"}),
                "Well scan must retain spatial field identities rather than claim time points");
        opticalRun.meta.erase("MODE");
        auto untimed = compactExport(opticalRun, fiKinetic);
        require(untimed.rows[untimed.dataHeaderRow - 1][0] == "Measurement" &&
                    untimed.rows[untimed.dataHeaderRow + 1][0] == "M1_1",
                "Incomplete timing data must fall back to field identities");
        auto duplicateKinetic = fiKinetic;
        duplicateKinetic.rows[1][2].clear();
        duplicateKinetic.rows.push_back(duplicateKinetic.rows[1]);
        fails([&] { compactExport(opticalRun, duplicateKinetic); });
        auto missingWavelength = kineticSpectrum;
        missingWavelength.rows[1][2].clear();
        fails([&] { compactExport(run, missingWavelength); });
        auto conflictingLabels = endpoint;
        conflictingLabels.rows.push_back({"A01", "2", "3.0", "Different label"});
        fails([&] { compactExport(run, conflictingLabels); });
        endpoint.rows[0][2].clear();
        endpoint.rows.push_back({"A01", "1", "9.99", "Sample A"});
        fails([&] { compactExport(run, endpoint); });
        require(safeName("A:B/C\\D? ") == "A_B_C_D_", "Filename sanitation");
        require(csv("a,\"b\n") == "\"a,\"\"b\n\"", "CSV quoting");
        require(decode(std::string("\xe6\xf8\xe5")) == "æøå", "Danish text encoding");
        require(isPlateWell("A01") && isPlateWell("AA01") && !isPlateWell("0ABS") &&
                    !isPlateWell("A1x"),
                "Plate-well recognition");
        require(measurementField("M1") && measurementField("M200") && measurementField("M1_716") &&
                    !measurementField("MODE") && !measurementField("M1_extra"),
                "Measurement-field recognition");
        auto partial = out / L"partial";
        fs::create_directory(partial);
        auto proposed = partial / a.path.filename();
        proposed += ".partial";
        {
            std::ofstream f(proposed);
            f << "keep me";
        }
        fails([&] { exportRun(run, partial); });
        require(fs::file_size(proposed) == 7, "Preexisting partial was modified");
        auto locked = out / L"locked";
        fs::create_directory(locked);
        auto lock = locked / a.path.filename();
        lock += ".partial.lock";
        fs::create_directory(lock);
        fails([&] { exportRun(run, locked); });
        require(fs::is_directory(lock), "Existing lock removed");
        auto broken = out / L"broken.dbf";
        fs::copy_file(run.data, broken);
        fs::resize_file(broken, 100);
        fails([&] { readDbf(broken); });
        auto deleted = out / L"deleted.dbf";
        fs::copy_file(run.data, deleted);
        {
            std::fstream f(deleted, std::ios::in | std::ios::out | std::ios::binary);
            unsigned char h[32];
            f.read((char *)h, 32);
            auto headerSize = unsigned(h[8]) | (unsigned(h[9]) << 8);
            auto recordSize = unsigned(h[10]) | (unsigned(h[11]) << 8);
            for (size_t index = 0;; ++index) {
                f.seekg(headerSize + index * recordSize);
                char marker = 0;
                f.get(marker);
                require(bool(f), "No active record to delete");
                if (marker != '*') {
                    f.seekp(headerSize + index * recordSize);
                    f.put('*');
                    break;
                }
            }
        }
        auto t = readDbf(deleted);
        require(t.rows.size() + 1 == original.rows.size() && t.deleted == original.deleted + 1,
                "Deleted records not skipped");
        fails([&] { readDbf(out / L"absent.dbf"); });
        auto x = exportRun(run, out, ExportFormat::Xlsx);
        require(x.path.filename() == "20261021_143000_Example Protocol.xlsx" &&
                    x.records == a.records,
                "XLSX filename must start with measurement date and time");
        auto x1 = exportRun(run, out, ExportFormat::Xlsx);
        auto x2 = exportRun(run, out, ExportFormat::Xlsx);
        require(x1.path.filename() == "20261021_143000_Example Protocol_1.xlsx" &&
                    x2.path.filename() == "20261021_143000_Example Protocol_2.xlsx",
                "Duplicate XLSX filenames must use sequential suffixes");
        require(columnName(0) == "A" && columnName(25) == "Z" && columnName(26) == "AA" &&
                    columnName(16383) == "XFD",
                "Excel column names");
        require(excelNumber("-123.4500") && !excelNumber("1234567890123456") &&
                    !excelNumber("=1+1"),
                "Excel number handling");
        std::ofstream special(out / "special.xlsx", std::ios::binary);
        writeXlsx(special, {"Label", "Value", "ID"}, {},
                  {{"æøå & <test>\r\n_x0041_", "12.3400", "00123"},
                   {"=1+1", "1234567890123456", "abc\x01"}},
                  {false, true, false});
        special.close();
        std::cout << "PASS: source parsing, compact export, dates, filenames, encoding, "
                     "CSV quoting, plate-well recognition, collisions, partial-file preservation, "
                     "locks, truncated/missing DBFs, deleted records\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

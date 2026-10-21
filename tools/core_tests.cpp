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
        auto a = exportRun(run, out), b = exportRun(run, out);
        require(a.path.filename() == "20261021_143000_Example Protocol.csv",
                "CSV filename must start with measurement date and time");
        require(a.path != b.path, "Collision overwrote export");
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

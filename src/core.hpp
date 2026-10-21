#pragma once
#include "xlsx.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
namespace fluo {
namespace fs = std::filesystem;
using Row = std::vector<std::string>;
inline std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \0", 0, 2);
    if (first == s.npos)
        return {};
    return s.substr(first, s.find_last_not_of(" \0", s.npos, 2) - first + 1);
}
inline void utf8(std::string &s, unsigned c) {
    if (c < 128)
        s += char(c);
    else if (c < 2048) {
        s += char(192 | (c >> 6));
        s += char(128 | (c & 63));
    } else {
        s += char(224 | (c >> 12));
        s += char(128 | ((c >> 6) & 63));
        s += char(128 | (c & 63));
    }
}
inline std::string decode(const std::string &s) {
    static const unsigned ext[32] = {0x20ac, 0x81,   0x201a, 0x192,  0x201e, 0x2026, 0x2020, 0x2021,
                                     0x2c6,  0x2030, 0x160,  0x2039, 0x152,  0x8d,   0x17d,  0x8f,
                                     0x90,   0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
                                     0x2dc,  0x2122, 0x161,  0x203a, 0x153,  0x9d,   0x17e,  0x178};
    std::string out;
    for (unsigned char c : s)
        utf8(out, c >= 128 && c < 160 ? ext[c - 128] : c);
    return out;
}
struct Field {
    std::string name;
    char type;
    unsigned size;
};
struct Table {
    std::vector<Field> fields;
    std::vector<Row> rows;
    unsigned deleted = 0;
    int index(const std::string &s) const {
        for (size_t i = 0; i < fields.size(); ++i)
            if (fields[i].name == s)
                return int(i);
        return -1;
    }
    std::string get(const Row &r, const std::string &s) const {
        int i = index(s);
        return i < 0 ? "" : r.at(i);
    }
};
inline Table readDbf(const fs::path &path, bool allowMemo = false) {
    auto size = fs::file_size(path);
    auto modified = fs::last_write_time(path);
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open DBF file");
    unsigned char head[32];
    if (!in.read((char *)head, 32))
        throw std::runtime_error("Truncated DBF header");
    if (head[0] != 3 && head[0] != 0x83 && head[0] != 0x8b)
        throw std::runtime_error("Unsupported DBF format");
    if (head[29] != 0x58 && head[29] != 0x03 && head[29] != 0x57 && head[29] != 0)
        throw std::runtime_error("Unsupported DBF text encoding");
    auto u16 = [](const unsigned char *p) { return unsigned(p[0]) | (unsigned(p[1]) << 8); };
    uint32_t count = uint32_t(head[4]) | (uint32_t(head[5]) << 8) | (uint32_t(head[6]) << 16) |
                     (uint32_t(head[7]) << 24);
    unsigned header = u16(head + 8), record = u16(head + 10);
    if (header < 33 || record < 1 || uint64_t(header) + uint64_t(count) * record > size)
        throw std::runtime_error("Incomplete DBF file; wait for the run to finish, then refresh");
    if (size > 512ULL * 1024 * 1024)
        throw std::runtime_error("DBF file exceeds supported size (512 MB)");
    Table t;
    unsigned total = 1;
    bool end = false;
    for (unsigned pos = 32; pos < header; pos += 32) {
        unsigned char f[32];
        if (!in.read((char *)f, 1))
            break;
        if (f[0] == 13) {
            end = true;
            break;
        }
        if (pos + 32 > header || !in.read((char *)f + 1, 31))
            throw std::runtime_error("Invalid DBF field descriptor");
        std::string name((char *)f, std::find(f, f + 11, 0) - f);
        if (!f[16] || name.empty() || std::string("CNFLDM").find(char(f[11])) == std::string::npos)
            throw std::runtime_error("Unsupported DBF field");
        if (f[11] == 'M' && !allowMemo)
            throw std::runtime_error("Measurement table has memo data; export is unsupported");
        if (t.index(name) >= 0)
            throw std::runtime_error("Duplicate DBF field");
        t.fields.push_back({name, char(f[11]), f[16]});
        total += f[16];
    }
    if (!end || total != record)
        throw std::runtime_error("Invalid DBF record layout");
    in.seekg(header);
    std::string buf(record, ' ');
    for (uint32_t j = 0; j < count; ++j) {
        if (!in.read(buf.data(), record))
            throw std::runtime_error("Truncated DBF record");
        if (buf[0] == '*') {
            ++t.deleted;
            continue;
        }
        if (buf[0] != ' ')
            throw std::runtime_error("Invalid DBF record marker");
        Row row;
        size_t p = 1;
        for (auto &f : t.fields) {
            auto s = trim(buf.substr(p, f.size));
            row.push_back(f.type == 'C' ? decode(s) : s);
            p += f.size;
        }
        t.rows.push_back(std::move(row));
    }
    if (fs::file_size(path) != size || fs::last_write_time(path) != modified)
        throw std::runtime_error("Database changed while reading; wait and refresh");
    return t;
}
struct Run {
    std::string id, protocol, stamp, technique, user, plate, protocolComment, id1, id2, id3;
    std::map<std::string, std::string> meta;
    fs::path data;
    bool available = false;
};
inline std::string technique(const std::string &s) {
    static const std::map<std::string, std::string> names = {{"0", "FI"},
                                                             {"1", "Absorbance"},
                                                             {"2", "FP"},
                                                             {"3", "Luminescence"},
                                                             {"4", "Dual luminescence"},
                                                             {"5", "Dual FI"},
                                                             {"6", "TRF (2 channels)"},
                                                             {"7", "AlphaScreen"},
                                                             {"8", "TRF"},
                                                             {"11", "Absorbance spectrum"},
                                                             {"12", "Nephelometry"}};
    auto it = names.find(s);
    return it == names.end() ? "Mode " + s : it->second;
}
inline std::string timestamp(std::string d, std::string t) {
    if (d.size() == 8)
        d = d.substr(0, 4) + "-" + d.substr(4, 2) + "-" + d.substr(6, 2);
    for (char &c : d)
        if (c == '.' || c == '/')
            c = '-';
    auto digits = [](const std::string &s) { return s.find_first_not_of("0123456789") == s.npos; };
    if (d.size() != 10 || d[4] != '-' || d[7] != '-' ||
        !digits(d.substr(0, 4) + d.substr(5, 2) + d.substr(8, 2)) || t.size() != 8 || t[2] != ':' ||
        t[5] != ':' || !digits(t.substr(0, 2) + t.substr(3, 2) + t.substr(6, 2)))
        throw std::runtime_error("Unrecognized run date/time: " + d + " " + t);
    int year = std::stoi(d.substr(0, 4)), month = std::stoi(d.substr(5, 2));
    int day = std::stoi(d.substr(8, 2));
    const int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (year == 0 || month < 1 || month > 12 || day < 1 ||
        day > monthDays[month - 1] + (month == 2 && leap ? 1 : 0) ||
        t.substr(0, 2) > "23" || t.substr(3, 2) > "59" || t.substr(6, 2) > "59")
        throw std::runtime_error("Invalid run date/time");
    return d + " " + t;
}
inline fs::path locate(fs::path p) {
    if (fs::is_regular_file(p))
        p = p.parent_path();
    for (auto suffix : {"", "Data", "User/Data", "Omega/User/Data", "BMG/Omega/User/Data"}) {
        auto q = p / fs::path(suffix);
        if (fs::exists(q / "Measure.dbf"))
            return q;
    }
    throw std::runtime_error(
        "Choose the folder containing Measure.dbf, usually BMG\\Omega\\User\\Data");
}
inline std::vector<Run> loadRuns(const fs::path &folder) {
    auto root = locate(folder);
    auto t = readDbf(root / "Measure.dbf", true);
    std::vector<Run> runs;
    for (auto name : {"MESSDATEI", "TSTNAME", "DATE", "TIME", "MESSTYP"})
        if (t.index(name) < 0)
            throw std::runtime_error("Unrecognized OMEGA run database");
    for (auto &r : t.rows) {
        Run run;
        run.id = t.get(r, "MESSDATEI");
        if (run.id.empty() || run.id.find_first_not_of("0123456789") != run.id.npos)
            throw std::runtime_error("Invalid run file ID");
        run.protocol = t.get(r, "TSTNAME");
        run.stamp = timestamp(t.get(r, "DATE"), t.get(r, "TIME"));
        run.technique = technique(t.get(r, "MESSTYP"));
        run.user = t.get(r, "USER");
        run.plate = t.get(r, "MPNAME");
        run.protocolComment = t.get(r, "KOMMENTAR");
        run.id1 = t.get(r, "IDNUMBER");
        run.id2 = t.get(r, "IDNUMBER2");
        run.id3 = t.get(r, "IDNUMBER3");
        run.data = root / (run.id + ".dbf");
        run.available = fs::is_regular_file(run.data);
        for (size_t i = 0; i < t.fields.size(); ++i)
            run.meta[t.fields[i].name] = r[i];
        runs.push_back(std::move(run));
    }
    std::stable_sort(runs.begin(), runs.end(), [](auto &a, auto &b) { return a.stamp > b.stamp; });
    return runs;
}
inline std::string safeName(std::string s) {
    for (char &c : s)
        if ((unsigned char)c < 32 || std::string("<>:\"/\\|?*").find(c) != std::string::npos)
            c = '_';
    while (!s.empty() && (s.back() == '.' || s.back() == ' '))
        s.pop_back();
    if (s.empty())
        s = "Unnamed protocol";
    // Keep complete UTF-8 code points when shortening for Windows paths.
    if (s.size() > 100) {
        size_t n = 100;
        while (n > 0 && ((unsigned char)s[n] & 0xc0) == 0x80)
            --n;
        s.resize(n);
    }
    return s;
}
inline std::string csv(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
inline void writeRow(std::ostream &out, const Row &row) {
    for (size_t i = 0; i < row.size(); ++i) {
        if (i)
            out << ',';
        out << csv(row[i]);
    }
    out << "\r\n";
}
struct Exported {
    fs::path path;
    size_t records;
};
enum class ExportFormat { Csv, Xlsx };

struct CompactExport {
    std::vector<Row> rows;
    std::vector<std::vector<bool>> numeric;
    size_t dataHeaderRow = 0;
    size_t measurements = 0;
};

inline bool isPlateWell(const std::string &well) {
    if (well.size() < 2)
        return false;
    size_t digit = 0;
    while (digit < well.size() && well[digit] >= 'A' && well[digit] <= 'Z')
        ++digit;
    return digit > 0 && digit < well.size() &&
           well.find_first_not_of("0123456789", digit) == std::string::npos;
}

inline bool measurementField(const std::string &name) {
    if (name.size() < 2 || name[0] != 'M')
        return false;
    size_t position = 1;
    while (position < name.size() && name[position] >= '0' && name[position] <= '9')
        ++position;
    if (position == 1)
        return false;
    if (position == name.size())
        return true;
    size_t suffix = position + 1;
    if (name[position] != '_' || suffix == name.size())
        return false;
    while (suffix < name.size() && name[suffix] >= '0' && name[suffix] <= '9')
        ++suffix;
    return suffix == name.size();
}

inline void addCompactRow(CompactExport &result, Row row, std::vector<bool> numeric = {}) {
    result.rows.push_back(std::move(row));
    result.numeric.push_back(std::move(numeric));
}

inline CompactExport compactExport(const Run &run, const Table &table) {
    CompactExport result;
    auto metadata = [&](const std::string &label, const std::string &value) {
        if (!value.empty())
            addCompactRow(result, {label, value});
    };
    metadata("Run ID", run.id);
    metadata("Protocol", run.protocol);
    metadata("Measurement date/time", run.stamp);
    metadata("Technique", run.technique);
    metadata("Plate", run.plate);
    metadata("User", run.user);
    metadata("ID1", run.id1);
    metadata("ID2", run.id2);
    metadata("ID3", run.id3);
    metadata("Protocol comment", run.protocolComment);
    addCompactRow(result, {});

    int wellIndex = table.index("WELLNUM"), contentIndex = table.index("CONTENT");
    int channelIndex = table.index("CH"), wavelengthIndex = table.index("WAVELENGTH");
    int timeIndex = table.index("TIME"), sampleIndex = table.index("SAMPLEID");
    std::vector<size_t> measurementIndexes;
    for (size_t i = 0; i < table.fields.size(); ++i)
        if (measurementField(table.fields[i].name))
            measurementIndexes.push_back(i);
    if (wellIndex < 0 || channelIndex < 0 || measurementIndexes.empty())
        throw std::runtime_error("Unrecognized measurement table");

    std::vector<const Row *> plateRows;
    for (const auto &row : table.rows)
        if (isPlateWell(row[size_t(wellIndex)]))
            plateRows.push_back(&row);
    if (plateRows.empty())
        throw std::runtime_error("Run contains no plate-well measurements");
    result.measurements = plateRows.size();

    if (measurementIndexes.size() == 1) {
        std::vector<std::string> wells;
        std::map<std::string, size_t> wellColumns;
        std::map<std::string, std::string> contents;
        for (const auto *row : plateRows) {
            const auto &well = (*row)[size_t(wellIndex)];
            if (!wellColumns.count(well)) {
                wellColumns[well] = wells.size();
                wells.push_back(well);
            }
            std::string content = sampleIndex >= 0 ? (*row)[size_t(sampleIndex)] : "";
            if (content.empty() && contentIndex >= 0)
                content = (*row)[size_t(contentIndex)];
            if (!content.empty())
                contents[well] = content;
        }

        bool useTime = false;
        if (wavelengthIndex < 0 && timeIndex >= 0) {
            std::set<std::string> seen;
            for (const auto *row : plateRows) {
                std::string key = (*row)[size_t(wellIndex)] + "\x1f" + (*row)[size_t(channelIndex)];
                if (!seen.insert(key).second) {
                    useTime = true;
                    break;
                }
            }
        }
        struct AxisRow {
            std::string primary, channel;
            std::vector<std::string> values;
            std::vector<bool> occupied;
        };
        std::vector<AxisRow> axes;
        std::map<std::string, size_t> axisIndexes;
        for (const auto *row : plateRows) {
            std::string primary;
            if (wavelengthIndex >= 0)
                primary = (*row)[size_t(wavelengthIndex)];
            else if (useTime)
                primary = (*row)[size_t(timeIndex)];
            else
                primary = (*row)[size_t(channelIndex)];
            std::string channel =
                (wavelengthIndex >= 0 || useTime) ? (*row)[size_t(channelIndex)] : "";
            std::string key = primary + "\x1f" + channel;
            auto inserted = axisIndexes.emplace(key, axes.size());
            if (inserted.second)
                axes.push_back({primary, channel, std::vector<std::string>(wells.size()),
                                std::vector<bool>(wells.size(), false)});
            auto &axis = axes[inserted.first->second];
            auto column = wellColumns[(*row)[size_t(wellIndex)]];
            if (axis.occupied[column])
                throw std::runtime_error("Run cannot be represented as a compact well matrix");
            axis.occupied[column] = true;
            axis.values[column] = (*row)[measurementIndexes[0]];
        }

        Row wellHeader = {"Well"};
        Row contentHeader = {"Content"};
        if (wavelengthIndex >= 0 || useTime) {
            wellHeader.push_back("");
            contentHeader.push_back("");
        }
        wellHeader.insert(wellHeader.end(), wells.begin(), wells.end());
        for (const auto &well : wells)
            contentHeader.push_back(contents[well]);
        addCompactRow(result, std::move(wellHeader));
        addCompactRow(result, std::move(contentHeader));
        Row header = {wavelengthIndex >= 0 ? "Wavelength (nm)" : (useTime ? "Time" : "Channel")};
        if (wavelengthIndex >= 0 || useTime)
            header.push_back("Channel");
        header.insert(header.end(), wells.begin(), wells.end());
        addCompactRow(result, std::move(header));
        result.dataHeaderRow = result.rows.size();
        for (const auto &axis : axes) {
            Row row = {axis.primary};
            if (wavelengthIndex >= 0 || useTime)
                row.push_back(axis.channel);
            row.insert(row.end(), axis.values.begin(), axis.values.end());
            addCompactRow(result, std::move(row),
                          std::vector<bool>(
                              1 + (wavelengthIndex >= 0 || useTime ? 1 : 0) + wells.size(), true));
        }
    } else {
        const Row *timeRow = nullptr;
        for (const auto &row : table.rows)
            if (row[size_t(channelIndex)] == "t") {
                timeRow = &row;
                break;
            }
        Row wellHeader = {"Well"}, contentHeader = {"Content"}, channelHeader = {"Channel"};
        for (const auto *row : plateRows) {
            wellHeader.push_back((*row)[size_t(wellIndex)]);
            std::string content = sampleIndex >= 0 ? (*row)[size_t(sampleIndex)] : "";
            if (content.empty() && contentIndex >= 0)
                content = (*row)[size_t(contentIndex)];
            contentHeader.push_back(content);
            channelHeader.push_back((*row)[size_t(channelIndex)]);
        }
        addCompactRow(result, std::move(wellHeader));
        addCompactRow(result, std::move(contentHeader));
        addCompactRow(result, std::move(channelHeader));
        Row header = {timeRow ? "Time" : "Measurement"};
        for (const auto *row : plateRows) {
            auto label = (*row)[size_t(wellIndex)];
            if ((*row)[size_t(channelIndex)] != "1")
                label += " (Ch " + (*row)[size_t(channelIndex)] + ")";
            header.push_back(label);
        }
        addCompactRow(result, std::move(header));
        result.dataHeaderRow = result.rows.size();
        for (size_t m = 0; m < measurementIndexes.size(); ++m) {
            Row row = {timeRow ? (*timeRow)[measurementIndexes[m]]
                               : table.fields[measurementIndexes[m]].name};
            for (const auto *source : plateRows)
                row.push_back((*source)[measurementIndexes[m]]);
            addCompactRow(result, std::move(row), std::vector<bool>(plateRows.size() + 1, true));
        }
    }
    return result;
}

inline Exported exportRun(const Run &run, const fs::path &output,
                          ExportFormat format = ExportFormat::Csv) {
    auto t = readDbf(run.data);
    if (t.index("WELLNUM") < 0 || t.index("CH") < 0 || t.index("M1") < 0)
        throw std::runtime_error("Unrecognized measurement table");
    if (t.rows.empty())
        throw std::runtime_error("Run has no active measurement records");
    fs::create_directories(output);
    std::string date = run.stamp.substr(0, 10);
    date.erase(std::remove(date.begin(), date.end(), '-'), date.end());
    std::string time = run.stamp.substr(11);
    time.erase(std::remove(time.begin(), time.end(), ':'), time.end());
    std::string stem = safeName(run.protocol) + "_" + date + "_" + time;
    std::string extension = format == ExportFormat::Xlsx ? ".xlsx" : ".csv";
    auto dest = output / fs::u8path(stem + extension);
    for (unsigned i = 0; fs::exists(dest); ++i)
        dest = output / fs::u8path(stem + "_run" + run.id + (i ? "_" + std::to_string(i + 1) : "") +
                                   extension);
    auto temporary = dest;
    temporary += ".partial";
    // Reserve a temporary name without opening or truncating an existing file.
    fs::path lock = temporary;
    lock += ".lock";
    if (!fs::create_directory(lock))
        throw std::runtime_error("Export already in progress for this run");
    bool ownsTemporary = false;
    try {
        if (fs::exists(temporary))
            throw std::runtime_error(
                "An unfinished .partial export exists; choose another output folder");
        ownsTemporary = true;
        std::ofstream out(temporary, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot create export in the output folder");
        auto compact = compactExport(run, t);
        if (format == ExportFormat::Xlsx) {
            writeMatrixXlsx(out, compact.rows, compact.numeric, compact.dataHeaderRow);
        } else {
            out << "\xef\xbb\xbf";
            for (const auto &row : compact.rows)
                writeRow(out, row);
        }
        out.flush();
        if (!out)
            throw std::runtime_error("Export write failed (check free space)");
        out.close();
        if (out.fail())
            throw std::runtime_error("Export close failed");
#ifdef _WIN32
        if (!MoveFileW(temporary.c_str(), dest.c_str()))
            throw std::runtime_error("Cannot finish export; destination may already exist");
#else
        if (::link(temporary.c_str(), dest.c_str()) != 0)
            throw std::runtime_error("Cannot finish export; destination may already exist");
        fs::remove(temporary);
#endif
        fs::remove(lock);
        return {dest, compact.measurements};
    } catch (...) {
        std::error_code ec;
        if (ownsTemporary)
            fs::remove(temporary, ec);
        fs::remove(lock, ec);
        throw;
    }
}
} // namespace fluo

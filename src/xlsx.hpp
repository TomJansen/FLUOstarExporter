#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fluo {
// Minimal streaming ZIP/SpreadsheetML writer; no Office installation or runtime DLLs.
class Xlsx {
    struct Entry {
        std::string name;
        uint32_t offset, size, crc;
    };
    std::ostream &out;
    std::vector<Entry> entries;
    uint32_t checksum = 0xffffffff;
    uint64_t written = 0;
    void le(uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i)
            out.put(char(value >> (8 * i)));
    }
    uint32_t position() {
        auto p = out.tellp();
        if (p < 0 || uint64_t(p) > 0xffffffffULL)
            throw std::runtime_error("Workbook exceeds ZIP32 size limit");
        return uint32_t(p);
    }

  public:
    explicit Xlsx(std::ostream &stream) : out(stream) {}
    void text(const std::string &s) {
        static const auto table = [] {
            std::vector<uint32_t> t(256);
            for (unsigned i = 0; i < 256; ++i) {
                uint32_t c = i;
                for (int j = 0; j < 8; ++j)
                    c = (c >> 1) ^ (0xedb88320U & uint32_t(-int(c & 1)));
                t[i] = c;
            }
            return t;
        }();
        for (unsigned char c : s)
            checksum = table[(checksum ^ c) & 255] ^ (checksum >> 8);
        written += s.size();
        if (written > 0xffffffffULL)
            throw std::runtime_error("Workbook XML exceeds ZIP32 size limit");
        out.write(s.data(), s.size());
    }
    void entry(const std::string &name, const std::function<void()> &body) {
        uint32_t offset = position();
        le(0x04034b50, 4);
        le(20, 2);
        le(0, 2);
        le(0, 2);
        le(0, 2);
        le(33, 2);
        le(0, 4);
        le(0, 4);
        le(0, 4);
        le(uint32_t(name.size()), 2);
        le(0, 2);
        out << name;
        checksum = 0xffffffff;
        written = 0;
        body();
        uint32_t end = position(), crc = checksum ^ 0xffffffff, size = uint32_t(written);
        out.seekp(offset + 14);
        le(crc, 4);
        le(size, 4);
        le(size, 4);
        out.seekp(end);
        entries.push_back({name, offset, size, crc});
    }
    void part(const std::string &name, const std::string &body) {
        entry(name, [&] { text(body); });
    }
    void finish() {
        auto offset = position();
        for (auto &e : entries) {
            le(0x02014b50, 4);
            le(20, 2);
            le(20, 2);
            le(0, 2);
            le(0, 2);
            le(0, 2);
            le(33, 2);
            le(e.crc, 4);
            le(e.size, 4);
            le(e.size, 4);
            le(uint32_t(e.name.size()), 2);
            le(0, 2);
            le(0, 2);
            le(0, 2);
            le(0, 2);
            le(0, 4);
            le(e.offset, 4);
            out << e.name;
        }
        auto end = position();
        le(0x06054b50, 4);
        le(0, 2);
        le(0, 2);
        le(uint32_t(entries.size()), 2);
        le(uint32_t(entries.size()), 2);
        le(end - offset, 4);
        le(offset, 4);
        le(0, 2);
    }
};
inline std::string xmlText(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if (c == '&')
            out += "&amp;";
        else if (c == '<')
            out += "&lt;";
        else if (c == '>')
            out += "&gt;";
        else if (c == '\r')
            out += "&#13;";
        else if (c < 32 && c != '\n' && c != '\t') {
            const char *hex = "0123456789ABCDEF";
            out += "_x00";
            out += hex[c >> 4];
            out += hex[c & 15];
            out += '_';
        } else if (c == '_' && i + 6 < s.size() && s[i + 1] == 'x' && s[i + 6] == '_')
            out += "_x005F_";
        else
            out += char(c);
    }
    return out;
}
inline std::string columnName(size_t column) {
    std::string s;
    do {
        s.insert(s.begin(), char('A' + column % 26));
        column = column / 26;
        if (!column)
            break;
        --column;
    } while (true);
    return s;
}
inline bool excelNumber(const std::string &s) {
    if (s.empty())
        return false;
    size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    bool dot = false, digit = false;
    unsigned significant = 0;
    bool started = false;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '.' && !dot) {
            dot = true;
            continue;
        }
        if (c < '0' || c > '9')
            return false;
        digit = true;
        if (c != '0')
            started = true;
        if (started)
            ++significant;
    }
    return digit && significant <= 15;
}
inline void writeXlsx(std::ostream &out, const std::vector<std::string> &header,
                      const std::vector<std::string> &metadata,
                      const std::vector<std::vector<std::string>> &rows,
                      const std::vector<bool> &numeric) {
    if (rows.size() + 1 > 1048576 || header.size() > 16384)
        throw std::runtime_error("Run exceeds Excel worksheet limits; choose CSV");
    Xlsx z(out);
    z.part(
        "[Content_Types].xml",
        R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)");
    z.part(
        "_rels/.rels",
        R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
    z.part(
        "xl/workbook.xml",
        R"(<?xml version="1.0" encoding="UTF-8"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Measurements" sheetId="1" r:id="rId1"/></sheets></workbook>)");
    z.part(
        "xl/_rels/workbook.xml.rels",
        R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>)");
    z.entry("xl/worksheets/sheet1.xml", [&] {
        z.text(
            R"(<?xml version="1.0" encoding="UTF-8"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><sheetData>)");
        auto row = [&](size_t index, const std::vector<std::string> &values, bool headings) {
            z.text("<row r=\"" + std::to_string(index) + "\">");
            for (size_t c = 0; c < values.size(); ++c) {
                const auto &v = values[c];
                if (v.empty())
                    continue;
                auto ref = columnName(c) + std::to_string(index);
                if (!headings && c < numeric.size() && numeric[c] && excelNumber(v))
                    z.text("<c r=\"" + ref + "\"><v>" + v + "</v></c>");
                else
                    z.text("<c r=\"" + ref + "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" +
                           xmlText(v) + "</t></is></c>");
            }
            z.text("</row>");
        };
        row(1, header, true);
        size_t index = 2;
        for (auto &r : rows) {
            auto joined = metadata;
            joined.insert(joined.end(), r.begin(), r.end());
            row(index++, joined, false);
        }
        z.text("</sheetData><autoFilter ref=\"A1:" + columnName(header.size() - 1) +
               std::to_string(rows.size() + 1) + "\"/></worksheet>");
    });
    z.finish();
}

inline void writeMatrixXlsx(std::ostream &out, const std::vector<std::vector<std::string>> &rows,
                            const std::vector<std::vector<bool>> &numeric, size_t dataHeaderRow) {
    size_t columns = 0;
    for (const auto &row : rows)
        columns = std::max(columns, row.size());
    if (rows.size() > 1048576 || columns > 16384)
        throw std::runtime_error("Run exceeds Excel worksheet limits; choose CSV");
    Xlsx z(out);
    z.part(
        "[Content_Types].xml",
        R"(<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/></Types>)");
    z.part(
        "_rels/.rels",
        R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
    z.part(
        "xl/workbook.xml",
        R"(<?xml version="1.0" encoding="UTF-8"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="Measurements" sheetId="1" r:id="rId1"/></sheets></workbook>)");
    z.part(
        "xl/_rels/workbook.xml.rels",
        R"(<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/></Relationships>)");
    z.entry("xl/worksheets/sheet1.xml", [&] {
        z.text(
            R"(<?xml version="1.0" encoding="UTF-8"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetViews><sheetView workbookViewId="0">)");
        if (dataHeaderRow && dataHeaderRow < rows.size())
            z.text("<pane ySplit=\"" + std::to_string(dataHeaderRow) + "\" topLeftCell=\"A" +
                   std::to_string(dataHeaderRow + 1) +
                   "\" activePane=\"bottomLeft\" state=\"frozen\"/>");
        z.text("</sheetView></sheetViews><sheetData>");
        for (size_t r = 0; r < rows.size(); ++r) {
            z.text("<row r=\"" + std::to_string(r + 1) + "\">");
            for (size_t c = 0; c < rows[r].size(); ++c) {
                const auto &value = rows[r][c];
                if (value.empty())
                    continue;
                auto ref = columnName(c) + std::to_string(r + 1);
                bool number = r < numeric.size() && c < numeric[r].size() && numeric[r][c] &&
                              excelNumber(value);
                if (number)
                    z.text("<c r=\"" + ref + "\"><v>" + value + "</v></c>");
                else
                    z.text("<c r=\"" + ref + "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" +
                           xmlText(value) + "</t></is></c>");
            }
            z.text("</row>");
        }
        z.text("</sheetData>");
        if (dataHeaderRow && columns)
            z.text("<autoFilter ref=\"A" + std::to_string(dataHeaderRow) + ":" +
                   columnName(columns - 1) + std::to_string(rows.size()) + "\"/>");
        z.text("</worksheet>");
    });
    z.finish();
}
} // namespace fluo

// Check every compact measurement value against its DBF coordinate without writing exports.
#include "../src/core.hpp"
#include <iostream>
using namespace fluo;
static void check(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
static size_t audit(const Run &run, const Table &table, const CompactExport &compact) {
    std::vector<size_t> points;
    for (size_t i = 0; i < table.fields.size(); ++i)
        if (measurementField(table.fields[i].name)) points.push_back(i);
    std::vector<const Row *> plate;
    std::map<std::string, size_t> wells;
    for (const auto &row : table.rows)
        if (isPlateWell(table.get(row, "WELLNUM"))) {
            plate.push_back(&row);
            if (!wells.count(table.get(row, "WELLNUM")))
                wells[table.get(row, "WELLNUM")] = wells.size();
        }
    const size_t first = compact.dataHeaderRow;
    const auto &header = compact.rows.at(first - 1);
    check(compact.numeric.size() == compact.rows.size(), "Numeric flags are misaligned");
    if (points.size() > 1) {
        check(compact.rows.size() - first == points.size(), "Measurement points missing or added");
        check(header.size() == plate.size() + 1, "Measurement columns missing or added");
        const Row *timing = nullptr;
        for (const auto &row : table.rows)
            if (table.get(row, "CH") == "t") timing = &row;
        if (runSetting(run, "MODE") == "50")
            check(header[0] == "Scan field", "Spatial scan mislabelled as time");
        for (size_t p = 0; p < points.size(); ++p) {
            const auto &output = compact.rows.at(first + p);
            check(output.size() == header.size(), "Ragged measurement row");
            const auto axis = header[0] == "Time" && timing ? (*timing)[points[p]]
                                                          : table.fields[points[p]].name;
            check(output[0] == axis && !axis.empty(), "Incorrect or blank time/field label");
            for (size_t w = 0; w < plate.size(); ++w)
                check(output[w + 1] == (*plate[w])[points[p]], "Value moved, altered, or omitted");
        }
    } else {
        check(points.size() == 1, "No measurement field");
        check(header.size() > wells.size(), "Missing measurement axes");
        size_t start = header.size() - wells.size();
        std::map<Row, size_t> axes;
        const bool wavelength = header[0] == "Wavelength (nm)", time = header[0] == "Time";
        for (size_t i = first; i < compact.rows.size(); ++i) {
            const auto &row = compact.rows[i];
            check(row.size() == header.size(), "Ragged measurement row");
            Row key = {row[0], wavelength || time ? row[1] : ""};
            check(axes.emplace(key, i).second, "Duplicate output axis");
        }
        // Read catalogue wavelength selections independently of channelOptics().
        std::vector<std::string> selections;
        std::istringstream filters(runSetting(run, "EXFILT"));
        std::string filter;
        while (std::getline(filters, filter, '\t')) selections.push_back(trim(filter));
        for (const auto *source : plate) {
            const auto channel = table.get(*source, "CH");
            std::string primary = channel;
            if (time) primary = table.get(*source, "TIME");
            if (wavelength) {
                primary = table.get(*source, "WAVELENGTH");
                if (table.index("WAVELENGTH") < 0) {
                    const auto selection = std::stoul(channel);
                    check(selection && selection <= selections.size(), "Cannot establish channel wavelength");
                    primary = selections[selection - 1];
                }
            }
            auto axis = axes.find({primary, wavelength || time ? channel : ""});
            check(axis != axes.end(), "Source coordinate missing from output");
            size_t column = start + wells.at(table.get(*source, "WELLNUM"));
            check(header[column] == table.get(*source, "WELLNUM"), "Well header misaligned");
            check(compact.rows[axis->second][column] == (*source)[points[0]],
                  "Value moved, altered, or omitted");
        }
    }
    return plate.size() * points.size();
}
int main(int argc, char **argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: audit SOURCE");
        auto runs = loadRuns(argv[1]);
        size_t verified = 0, missing = 0, failed = 0, values = 0;
        std::set<std::string> schemas;
        for (const auto &run : runs) {
            if (!run.available) { ++missing; std::cout << "Missing DBF: " << run.id << "\n"; continue; }
            try {
                auto table = readDbf(run.data);
                std::string schema;
                for (const auto &field : table.fields)
                    schema += field.name + ":" + field.type + ":" + std::to_string(field.size) + ";";
                schemas.insert(schema);
                values += audit(run, table, compactExport(run, table));
                ++verified;
            } catch (const std::exception &error) {
                ++failed;
                std::cerr << "Run " << run.id << ": " << error.what() << "\n";
            }
        }
        std::cout << verified << " files verified; " << schemas.size() << " layouts; " << values
                  << " exact source values checked; " << missing << " missing files; " << failed << " failures\n";
        return failed ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}

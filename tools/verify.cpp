#include "../src/core.hpp"
#include <iostream>
#include <set>
int main(int argc, char **argv) {
    try {
        if (argc != 3 && argc != 4)
            throw std::runtime_error("Usage: verify SOURCE OUTPUT");
        auto runs = fluo::loadRuns(argv[1]);
        bool xlsx = argc == 4;
        std::set<std::string> schemas;
        size_t records = 0, fail = 0;
        std::ofstream manifest(std::filesystem::path(argv[2]).parent_path() /
                               (xlsx ? "fluo-xlsx-manifest.csv" : "fluo-verify-manifest.csv"));
        fluo::writeRow(manifest, {"RunID", "Source", "CSV", "Records"});
        for (auto &r : runs) {
            try {
                if (xlsx) {
                    auto table = fluo::readDbf(r.data);
                    std::string key;
                    for (auto &f : table.fields)
                        key += f.name + ":" + f.type + ":" + std::to_string(f.size) + ";";
                    if (!schemas.insert(key).second)
                        continue;
                }
                auto out = fluo::exportRun(
                    r, argv[2], xlsx ? fluo::ExportFormat::Xlsx : fluo::ExportFormat::Csv);
                records += out.records;
                fluo::writeRow(manifest, {r.id, r.data.string(), out.path.string(),
                                          std::to_string(out.records)});
            } catch (const std::exception &e) {
                ++fail;
                std::cerr << r.id << ": " << e.what() << "\n";
            }
        }
        std::cout << runs.size() << " runs; " << records << " records; " << fail << " failed\n";
        return fail ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}

#include <newton/analytics/group_by.hpp>
#include <newton/memory/mapped_file.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
int main() {
    const auto path = std::filesystem::temp_directory_path() / "newton-kernels-integration.csv";
    { std::ofstream file(path, std::ios::binary); file << "region,value\nwest,4\neast,5\nwest,9\n"; }
    try {
        newton::memory::MappedFile input(path);
        newton::parsing::CsvParser csv;
        std::vector<std::string> regions;
        const auto rows = csv.parse(input.text(), [&](const auto& row) { if (row[0] != "region") regions.push_back(row[0]); });
        if (rows != 4) throw std::runtime_error("unexpected rows");
        const auto counts = newton::analytics::group_count(regions, [](const auto& r) { return r; });
        if (counts.at("west") != 2 || counts.at("east") != 1) throw std::runtime_error("incorrect group count");
    } catch (...) { std::filesystem::remove(path); throw; }
    std::filesystem::remove(path);
    std::cout << "PASS mapped CSV -> grouped counts\n";
}

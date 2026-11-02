#include <newton/memory/mapped_file.hpp>
#include <newton/parsing/csv_parser.hpp>
#include <iostream>
#include <stdexcept>
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: newton_csv_example input.csv\n"; return 2; }
    try {
        newton::memory::MappedFile input(argv[1]);
        const auto rows = newton::parsing::CsvParser{}.parse(input.text(), [](const auto& row) {
            std::cout << "fields: " << row.size() << '\n';
        });
        std::cout << "rows: " << rows << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}

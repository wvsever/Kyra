// Kyra pipeline stages: generate -> aggregate -> export (docs/ARCHITECTURE.md, pipeline).
#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "kyra/book.hpp"

namespace kyra {

// One reported cell in FactsConverter terms (regkey from the AGU_*_i.xltx Regkey map).
struct Fact {
    std::string sheet, regkey;
    int ordinal = 0;               // open (growing list) tables: row number from 1; closed tables: 0
    std::string value;
};

struct Module {                    // one reporting set, ready for export
    std::string name;              // "aml_eligibility"
    std::string family;            // FC file suffix, "AML_ELIGIBILITY"
    std::string workbook;          // "AGU_AML_ELIGIBILITY_v100_i.xltx"
    std::vector<Fact> facts;
};

// Stage 1. Deterministic for (profile, seed). Throws std::invalid_argument for an unknown profile.
Book generate(const std::string& profile, const std::string& ref_date, std::uint64_t seed);
void enrich_aml(Book& b, std::uint64_t seed);                    // second stream for aml_ra (K013), called by generate
std::string lei_with_check_digits(const std::string& prefix18);   // ISO 17442 (ISO 7064 MOD 97-10)

// Stage 2.
Module aggregate_eligibility(const Book& b);
Module aggregate_ra(const Book& b);   // aml_ra_v110, 34 templates (engine/src/ra.cpp)

// Stage 3. Writes <dir>/fc_import_<family>.csv; returns the path.
std::filesystem::path export_fc_csv(const Book& b, const Module& m, const std::filesystem::path& dir);
std::string fc_csv_text(const Book& b, const Module& m);

// Inspection output: <dir>/{units,customers,transactions}.csv.
void write_book_csv(const Book& b, const std::filesystem::path& dir);

}  // namespace kyra

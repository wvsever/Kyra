// kyra: synthetic AML data -> EBA 4.4 AMLA reporting sets -> FactsConverter CSV. Offline, no DB.
// --version / --version-json follow lanetools docs/product_versioning.md.
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

#include "kyra/pipeline.hpp"
#include "product_version.h"

namespace {

const char* kUsage =
    "usage: kyra <command> [options]\n"
    "commands:\n"
    "  run       generate + aggregate + export: --out <dir> [--entity demo] [--ref-date 2026-12-31] [--seed 42]\n"
    "            [--module aml_eligibility|aml_ra]\n"
    "  generate  write the book only (<out>/book/*.csv), same options\n"
    "exit: 0 ok, 1 failure, 2 usage, 3 stage not implemented\n";

int usage(const char* why) {
    if (why) std::fprintf(stderr, "kyra: %s\n", why);
    std::fputs(kUsage, stderr);
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 2) {
        int v = product_version::handle(argv[1]);
        if (v >= 0) return v;
    }
    if (argc < 2) return usage(nullptr);
    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h") { std::fputs(kUsage, stdout); return 0; }
    if (cmd != "run" && cmd != "generate") return usage(("unknown command: " + cmd).c_str());

    std::string out, entity = "demo", ref = "2026-12-31", module = "aml_eligibility";
    unsigned long long seed = 42;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (i + 1 >= argc) return usage(("missing value for " + a).c_str());
        std::string v = argv[++i];
        if (a == "--out") out = v;
        else if (a == "--entity") entity = v;
        else if (a == "--ref-date") ref = v;
        else if (a == "--seed") seed = std::strtoull(v.c_str(), nullptr, 10);
        else if (a == "--module") module = v;
        else return usage(("unknown option: " + a).c_str());
    }
    if (out.empty()) return usage("--out is required");
    if (module != "aml_eligibility" && module != "aml_ra") return usage(("unknown module: " + module).c_str());

    try {
        kyra::Book b = kyra::generate(entity, ref, seed);
        kyra::write_book_csv(b, std::filesystem::path(out) / "book");
        std::printf("kyra: book %zu units, %zu customers, %zu transactions\n", b.units.size(), b.customers.size(),
                    b.transactions.size());
        if (cmd == "generate") return 0;
        kyra::Module m = module == "aml_ra" ? kyra::aggregate_ra(b) : kyra::aggregate_eligibility(b);
        auto p = kyra::export_fc_csv(b, m, std::filesystem::path(out) / "factsconverter");
        std::printf("kyra: %s %zu facts -> %s\n", m.name.c_str(), m.facts.size(), p.generic_string().c_str());
        return 0;
    } catch (const std::logic_error& e) {
        std::fprintf(stderr, "kyra: %s\n", e.what());
        return dynamic_cast<const std::invalid_argument*>(&e) ? 2 : 3;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "kyra: %s\n", e.what());
        return 1;
    }
}

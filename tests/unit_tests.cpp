// Kyra unit tests (dependency-free). Usage: kyra_unit_tests <regkeys file>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>

#include "kyra/pipeline.hpp"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++fails; } } while (0)

static int lei_mod97(const std::string& lei) {
    int mod = 0;
    for (char c : lei) mod = (c >= 'A') ? (mod * 100 + (c - 'A' + 10)) % 97 : (mod * 10 + (c - '0')) % 97;
    return mod;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::puts("usage: kyra_unit_tests <regkeys file>"); return 2; }
    std::map<std::string, std::string> type_of;   // regkey -> datatype
    std::ifstream rk(argv[1]);
    for (std::string l; std::getline(rk, l);) {
        if (l.empty() || l[0] == '#') continue;
        type_of[l.substr(0, l.find('|'))] = l.substr(l.rfind('|') + 1);
    }
    CHECK(type_of.size() == 54);

    // LEI: ISO 17442 check digits, rule v904861_m pattern.
    std::string lei = kyra::lei_with_check_digits("KYRA00DEMOBANKBE00");
    CHECK(lei.size() == 20 && lei_mod97(lei) == 1);

    // Determinism.
    kyra::Book a = kyra::generate("demo", "2026-12-31", 42), b = kyra::generate("demo", "2026-12-31", 42);
    CHECK(a.transactions.size() == b.transactions.size() && kyra::fc_csv_text(a, kyra::aggregate_eligibility(a)) ==
                                                                 kyra::fc_csv_text(b, kyra::aggregate_eligibility(b)));
    for (const auto& t : a.transactions) CHECK(t.amount_eur >= 10.0);

    kyra::Module m = kyra::aggregate_eligibility(a);
    std::map<std::string, std::string> closed;    // closed-table regkey -> value
    std::set<std::string> v0401_rows;
    for (const auto& f : m.facts) {
        CHECK(type_of.count(f.regkey) == 1);       // every fact has a regkey in the FC map
        CHECK(!f.value.empty() && f.value != "0" && f.value != "0.00");   // no zeros as data
        const std::string& t = type_of[f.regkey];
        if (t == "BOOL") CHECK(f.value == "true" || f.value == "false");
        if (t == "INTEGER" || t == "AMOUNT") CHECK(std::stod(f.value) > 0);
        if (!f.ordinal) closed[f.regkey] = f.value;
        if (f.sheet == "V_04.01") v0401_rows.insert(std::to_string(f.ordinal));
        CHECK(f.sheet != "V_02.02");                // v904921_m: no head office outside the EU
    }
    // v904892_m / v905231_m: FPS -> count of member states in 1..27.
    CHECK(closed["TV_01.01-L0010-C0040"] == "true");
    int n = std::stoi(closed["TV_01.01-L0010-C0050"]);
    CHECK(n >= 1 && n <= 27);
    CHECK(closed.count("TV_02.01-L0010-C0030") == 1);   // v904889_m
    CHECK(!v0401_rows.empty());
    int per_row = 0;                                     // v904895/6_m: every V_04.01 row has both amounts
    for (const auto& f : m.facts) if (f.sheet == "V_04.01" && (f.regkey.ends_with("C0050") || f.regkey.ends_with("C0060"))) ++per_row;
    CHECK(per_row == int(2 * v0401_rows.size()));

    bool threw = false;
    try { kyra::aggregate_ra(a); } catch (const std::logic_error&) { threw = true; }
    CHECK(threw);

    std::printf("%s (%d failures, %zu facts)\n", fails ? "FAIL" : "ok", fails, m.facts.size());
    return fails ? 1 : 0;
}

// Kyra unit tests (dependency-free). Usage: kyra_unit_tests <eligibility regkeys> <aml_ra regkeys>
#include <cmath>
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
    int n0202 = 0;
    for (const auto& f : m.facts) {
        CHECK(type_of.count(f.regkey) == 1);       // every fact has a regkey in the FC map
        CHECK(!f.value.empty() && f.value != "0" && f.value != "0.00");   // no zeros as data
        const std::string& t = type_of[f.regkey];
        if (t == "BOOL") CHECK(f.value == "true" || f.value == "false");
        if (t == "INTEGER" || t == "AMOUNT") CHECK(std::stod(f.value) > 0);
        if (!f.ordinal) closed[f.regkey] = f.value;
        if (f.sheet == "V_04.01") v0401_rows.insert(std::to_string(f.ordinal));
        n0202 += f.sheet == "V_02.02";
    }
    // v904892_m / v905231_m: FPS -> count of member states in 1..27.
    CHECK(closed["TV_01.01-L0010-C0040"] == "true");
    int n = std::stoi(closed["TV_01.01-L0010-C0050"]);
    CHECK(n >= 1 && n <= 27);
    CHECK(closed.count("TV_02.01-L0010-C0030") == 1);   // v904889_m
    CHECK(!v0401_rows.empty());
    CHECK(n0202 == 4 && closed["TV_01.01-L0010-C0020"] == "true");   // v904893_m/v904921_m: head office filed
    int per_row = 0;                                     // v904895/6_m: every V_04.01 row has both amounts
    for (const auto& f : m.facts) if (f.sheet == "V_04.01" && (f.regkey.ends_with("C0050") || f.regkey.ends_with("C0060"))) ++per_row;
    CHECK(per_row == int(2 * v0401_rows.size()));

    // aml_ra: keys, types, no zeros, every template, and the cross-template rules the generator must honour.
    std::map<std::string, std::string> ra_type;
    if (argc >= 3) {
        std::ifstream rr(argv[2]);
        for (std::string l; std::getline(rr, l);) {
            if (l.empty() || l[0] == '#') continue;
            ra_type[l.substr(0, l.find('|'))] = l.substr(l.rfind('|') + 1);
        }
    }
    CHECK(ra_type.size() == 261);
    kyra::Module ra = kyra::aggregate_ra(a);
    CHECK(kyra::fc_csv_text(a, ra) == kyra::fc_csv_text(b, kyra::aggregate_ra(b)));
    {   // K025: demo_manual files c0030 and leaves the automated cells c0040-c0120 empty
        kyra::Book mb = kyra::generate("demo_manual", "2026-12-31", 42);
        bool c30 = false, auto_cell = false;
        for (const auto& f : kyra::aggregate_ra(mb).facts) {
            if (f.sheet != "W_07.05") continue;
            std::string col = f.regkey.substr(f.regkey.size() - 5);
            if (col == "C0030") c30 = true;
            if (col >= "C0040" && col <= "C0120") auto_cell = true;
        }
        CHECK(c30 && !auto_cell);
    }
    std::set<std::string> sheets;
    std::map<std::string, double> v;                      // closed cells "W_02.01 C0010" -> value
    std::map<std::string, double> colsum;                  // open-table column sums "W_05.01 C0020"
    std::map<std::pair<int, std::string>, double> geo;     // (W_05.01 row, column) -> value
    for (const auto& f : ra.facts) {
        CHECK(ra_type.count(f.regkey) == 1);
        CHECK(!f.value.empty() && f.value != "0" && f.value != "0.00");
        const std::string& t = ra_type[f.regkey];
        if (t == "BOOL") CHECK(f.value == "true" || f.value == "false");
        if (t == "PERCENTAGE") CHECK(std::stod(f.value) > 0 && std::stod(f.value) <= 1);
        if (t == "DATE") CHECK(f.value <= a.ref_date);
        sheets.insert(f.sheet);
        std::string col = f.regkey.substr(f.regkey.size() - 5);
        bool numeric = t == "INTEGER" || t == "AMOUNT" || t == "DECIMAL" || t == "PERCENTAGE";
        if (numeric) CHECK(std::stod(f.value) > 0);
        if (numeric && !f.ordinal) v[f.sheet + " " + col] = std::stod(f.value);
        if (numeric && f.ordinal) colsum[f.sheet + " " + col] += std::stod(f.value);
        if (numeric && f.sheet == "W_05.01") geo[{f.ordinal, col}] = std::stod(f.value);
    }
    CHECK(sheets.size() == 34);
    auto near = [](double x, double y) { return std::fabs(x - y) < 0.01 * (1 + std::fabs(y)); };
    CHECK(v["W_02.01 C0010"] == v["W_02.01 C0020"] + v["W_02.01 C0030"]);                        // v903768
    CHECK(v["W_02.01 C0040"] + v["W_02.01 C0050"] <= v["W_02.01 C0010"]);                         // v904930
    CHECK(v["W_02.01 C0080"] + v["W_02.01 C0090"] <= v["W_02.01 C0070"]);                         // v904964
    CHECK(v["W_02.01 C0130"] <= v["W_02.01 C0030"] && v["W_02.01 C0110"] <= v["W_02.01 C0030"]);  // v903804
    for (const char* c : {"C0020", "C0030", "C0040", "C0050"})                                    // v904976-9
        CHECK(colsum[std::string("W_05.01 ") + c] == v[std::string("W_02.01 ") + c]);
    CHECK(colsum["W_05.01 C0110"] == v["W_04.11 C0010"] + v["W_04.11 C0020"] + v["W_04.11 C0040"] + v["W_04.11 C0050"]);
    CHECK(near(colsum["W_05.01 C0120"], v["W_04.01 C0100"]));                                     // v904981
    CHECK(near(colsum["W_05.01 C0180"], v["W_04.02 C0030"]));                                     // v904982
    CHECK(near(colsum["W_05.01 C0190"], v["W_04.02 C0040"]));                                     // v904983
    auto g = [&](int r, const char* c) { auto it = geo.find(std::make_pair(r, std::string(c))); return it == geo.end() ? 0.0 : it->second; };
    int geo_rows = 0;
    for (const auto& f : ra.facts) if (f.sheet == "W_05.01" && f.ordinal > geo_rows) geo_rows = f.ordinal;
    // v903846 (c0110 = c0020 + c0030) is a broken EBA rule (docs/broken_rules.csv): fund investors are a subset (K018).
    double fund_total = v["W_04.11 C0010"] + v["W_04.11 C0020"] + v["W_04.11 C0040"] + v["W_04.11 C0050"];
    CHECK(fund_total > 0 && fund_total < 0.15 * v["W_02.01 C0010"]);
    for (int r = 1; r <= geo_rows; ++r) CHECK(g(r, "C0110") <= g(r, "C0020") + g(r, "C0030"));
    CHECK(v["W_07.03 C0040"] + v["W_07.03 C0050"] + v["W_07.03 C0060"] + v["W_07.03 C0070"] <= v["W_02.01 C0010"]);
    CHECK(v["W_07.07 C0080"] > 0);                                                                // v904610_s_2, K024
    CHECK(v["W_07.04 C0070"] <= v["W_07.04 C0060"]);                                           // v904974
    CHECK(v["W_03.04 C0040"] <= v["W_03.04 C0030"] && v["W_03.04 C0130"] <= v["W_03.04 C0120"]);
    CHECK(v["W_03.06 C0030"] <= v["W_03.06 C0010"] && v["W_03.06 C0040"] <= v["W_03.06 C0020"]);  // v903826
    CHECK(v["W_04.10 C0120"] + v["W_04.10 C0130"] <= v["W_04.10 C0110"]);                         // v904958
    CHECK(v["W_04.10 C0170"] + v["W_04.10 C0180"] <= v["W_04.10 C0160"]);                         // v904960
    CHECK(v["W_04.11 C0010"] + v["W_04.11 C0020"] <= v["W_02.01 C0010"]);                         // v903822
    CHECK(v["W_04.07 C0010"] <= v["W_02.01 C0030"] && v["W_07.06 C0020"] <= v["W_07.06 C0030"]);
    CHECK(v.count("W_07.05 C0030") == 0);                                                         // v903748

    std::printf("%s (%d failures, %zu + %zu facts)\n", fails ? "FAIL" : "ok", fails, m.facts.size(), ra.facts.size());
    return fails ? 1 : 0;
}

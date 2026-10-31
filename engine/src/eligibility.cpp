// Stage 2, aml_eligibility_v100: book -> FactsConverter facts (template -> source map in docs/ARCHITECTURE.md).
// Regkeys from M:/WIP/EBA/EBA 4.4 fase 1/aml_eligibility_v100/Regkey (V_00.01 has none there: TRAPS T1).
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>

#include "kyra/pipeline.hpp"

namespace kyra {
namespace {

std::string b2s(bool v) { return v ? "true" : "false"; }
std::string money(double v) { char s[48]; std::snprintf(s, sizeof s, "%.2f", v); return s; }

struct Out {
    Module& m;
    void cell(const char* sheet, const char* row, const char* col, const std::string& v, int ordinal = 0) {
        if (v.empty()) return;     // blank by rule only (conditional "(Other)" columns, K007)
        m.facts.push_back({sheet, std::string("T") + sheet + "-" + row + "-" + col, ordinal, v});
    }
};

struct FpsRow { std::size_t customers = 0; double in = 0, out = 0; };

}  // namespace

Module aggregate_eligibility(const Book& b) {
    if (b.units.empty()) throw std::invalid_argument("book has no reporting obliged entity");
    Module m{"aml_eligibility", "AML_ELIGIBILITY", "AGU_AML_ELIGIBILITY_v100_i.xltx", {}};
    Out o{m};
    const Unit& e = b.units[0];

    // FPS (K004): a customer resident in an EU state other than the booking unit's state.
    static const std::set<std::string> eu = {"AUSTRIA", "BELGIUM", "BULGARIA", "CROATIA", "CYPRUS", "CZECHIA",
        "DENMARK", "ESTONIA", "FINLAND", "FRANCE", "GERMANY", "GREECE", "HUNGARY", "IRELAND", "ITALY", "LATVIA",
        "LITHUANIA", "LUXEMBOURG", "MALTA", "NETHERLANDS", "POLAND", "PORTUGAL", "ROMANIA", "SLOVAKIA", "SLOVENIA",
        "SPAIN", "SWEDEN"};
    std::map<std::pair<std::size_t, std::string>, FpsRow> fps;   // (unit, state served) -> totals
    std::map<std::uint32_t, std::pair<std::size_t, std::string>> key_of;
    for (const auto& c : b.customers) {
        const Unit& u = b.units.at(c.unit);
        if (!u.fps || c.residence == u.country || !eu.count(c.residence)) continue;
        auto k = std::make_pair(std::size_t(c.unit), c.residence);
        fps[k].customers++;
        key_of[c.id] = k;
    }
    for (const auto& t : b.transactions) {
        auto it = key_of.find(t.customer);
        if (it == key_of.end()) continue;
        (t.incoming ? fps[it->second].in : fps[it->second].out) += t.amount_eur;
    }
    std::map<std::size_t, std::set<std::string>> states;          // unit -> FPS states
    for (const auto& [k, v] : fps) states[k.first].insert(k.second);
    std::set<std::string> entity_states;                          // head office + branches (K004)
    for (const auto& [u, s] : states)
        if (u == 0 || b.units[u].structure == "Branch") entity_states.insert(s.begin(), s.end());

    // V_00.02: the entity is its own designated reporting obliged entity (K002).
    o.cell("V_00.02", "L0010", "C0010", e.lei);
    o.cell("V_00.02", "L0010", "C0020", e.national_code);
    o.cell("V_00.02", "L0010", "C0030", e.name);
    o.cell("V_00.02", "L0010", "C0040", e.supervisor);
    o.cell("V_00.02", "L0010", "C0060", e.country);

    bool has_est = b.units.size() > 1;
    o.cell("V_01.01", "L0010", "C0010", b2s(b.has_eu_parent));
    o.cell("V_01.01", "L0010", "C0020", b2s(b.head_office_outside_eu));
    o.cell("V_01.01", "L0010", "C0030", b2s(has_est));
    o.cell("V_01.01", "L0010", "C0040", b2s(!entity_states.empty()));
    if (!entity_states.empty()) o.cell("V_01.01", "L0010", "C0050", std::to_string(entity_states.size()));

    if (b.has_eu_parent) {
        const Parent& p = b.parent;
        o.cell("V_02.01", "L0010", "C0010", p.lei);
        o.cell("V_02.01", "L0010", "C0020", p.national_code);
        o.cell("V_02.01", "L0010", "C0030", p.name);
        o.cell("V_02.01", "L0010", "C0040", p.name_latin);
        o.cell("V_02.01", "L0010", "C0050", b2s(p.aml_supervised));
        o.cell("V_02.01", "L0010", "C0060", p.country);
    }
    if (b.head_office_outside_eu) {   // V_02.02 only for a non-EU head office (profile demo_third; exclusive with V_02.01)
        const Parent& p = b.parent;
        o.cell("V_02.02", "L0010", "C0010", p.lei);
        o.cell("V_02.02", "L0010", "C0020", p.national_code);
        o.cell("V_02.02", "L0010", "C0030", p.name);
        o.cell("V_02.02", "L0010", "C0040", p.country);
    }

    int row = 0;
    for (std::size_t i = 1; i < b.units.size(); ++i) {
        const Unit& u = b.units[i];
        ++row;
        o.cell("V_03.01", "L005", "C0030", u.name, row);
        o.cell("V_03.01", "L005", "C0070", u.country, row);
        o.cell("V_03.01", "L005", "C0010", u.lei, row);
        o.cell("V_03.01", "L005", "C0020", u.national_code, row);
        o.cell("V_03.01", "L005", "C0040", u.name_latin, row);
        o.cell("V_03.01", "L005", "C0050", u.category, row);
        o.cell("V_03.01", "L005", "C0080", u.supervisor, row);
        o.cell("V_03.01", "L005", "C0100", u.add_supervisor, row);
        o.cell("V_03.01", "L005", "C0120", u.structure, row);
        bool f = states.count(i) > 0;
        o.cell("V_03.01", "L005", "C0140", b2s(f), row);
        if (f) o.cell("V_03.01", "L005", "C0150", std::to_string(states[i].size()), row);
    }

    row = 0;
    for (const auto& [k, v] : fps) {
        const Unit& u = b.units[k.first];
        ++row;
        o.cell("V_04.01", "L005", "C0010", u.name, row);
        o.cell("V_04.01", "L005", "C0020", k.second, row);    // state where services are provided (K008)
        o.cell("V_04.01", "L005", "C0030", u.country, row);   // state of the reported establishment (K008)
        o.cell("V_04.01", "L005", "C0040", std::to_string(v.customers), row);
        o.cell("V_04.01", "L005", "C0050", money(v.in), row);
        o.cell("V_04.01", "L005", "C0060", money(v.out), row);
    }
    return m;
}

}  // namespace kyra

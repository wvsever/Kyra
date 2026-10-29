// Stage 3: FactsConverter regkey import CSV (same layout as Vera's --emit-fc-csv), plus book inspection CSVs.
// Line = fields joined by ";" with quotes, CRLF: first field unquoted at the start, every line ends in ";".
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "kyra/pipeline.hpp"

namespace kyra {
namespace {

std::string fc_line(const std::vector<std::string>& f) {
    std::string s;
    for (const auto& x : f) {
        if (x.find('"') != std::string::npos) throw std::invalid_argument("FC CSV value holds a quote: " + x);
        s += x + "\";\"";
    }
    return s + "\r\n";
}

void write_file(const std::filesystem::path& p, const std::string& text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f << text;
    if (!f) throw std::runtime_error("cannot write " + p.string());
}

std::string csv(const std::string& v) { return v.find(',') == std::string::npos ? v : "\"" + v + "\""; }

}  // namespace

std::string fc_csv_text(const Book& b, const Module& m) {
    if (b.ref_date.size() != 10) throw std::invalid_argument("ref date must be YYYY-MM-DD");
    std::string month = std::to_string(std::stoi(b.ref_date.substr(5, 2)));
    std::string s = fc_line({b.units.at(0).lei, b.ref_date.substr(0, 4), month, "Individual", "", "x1",
                             b.ref_date + "T00:00:00.0000000", m.workbook});
    for (const auto& f : m.facts)
        s += fc_line({f.sheet, f.regkey, "", f.ordinal ? std::to_string(f.ordinal) : "", f.value, ""});
    return s;
}

std::filesystem::path export_fc_csv(const Book& b, const Module& m, const std::filesystem::path& dir) {
    auto p = dir / ("fc_import_" + m.family + ".csv");
    write_file(p, fc_csv_text(b, m));
    return p;
}

void write_book_csv(const Book& b, const std::filesystem::path& dir) {
    std::ostringstream u, c, t;
    u << "unit,lei,name,category,country,structure,fps\n";
    for (std::size_t i = 0; i < b.units.size(); ++i) {
        const Unit& x = b.units[i];
        u << i << ',' << x.lei << ',' << csv(x.name) << ',' << x.category << ',' << x.country << ','
          << x.structure << ',' << (x.fps ? "true" : "false") << '\n';
    }
    const char* rc[] = {"low", "medium", "high"};
    c << "customer,unit,residence,risk_class,product,channel,pep\n";
    for (const auto& x : b.customers)
        c << x.id << ',' << x.unit << ',' << x.residence << ',' << rc[int(x.risk)] << ',' << x.product << ','
          << x.channel << ',' << (x.pep ? "true" : "false") << '\n';
    t << "customer,direction,amount_eur,counterparty_country\n";
    for (const auto& x : b.transactions) {
        char a[48];
        std::snprintf(a, sizeof a, "%.2f", x.amount_eur);
        t << x.customer << ',' << (x.incoming ? "in" : "out") << ',' << a << ',' << x.counterparty_country << '\n';
    }
    write_file(dir / "units.csv", u.str());
    write_file(dir / "customers.csv", c.str());
    write_file(dir / "transactions.csv", t.str());
}

}  // namespace kyra

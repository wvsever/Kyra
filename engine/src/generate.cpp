// Stage 1: synthetic AML book. Own PRNG (splitmix64) so the book is identical on every compiler.
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include "kyra/pipeline.hpp"

namespace kyra {
namespace {

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uniform() { return double(next() >> 11) * (1.0 / 9007199254740992.0); }   // [0,1)
    int below(int n) { return int(next() % std::uint64_t(n)); }
    // Lognormal amount (K006): median exp(mu), never below 10 EUR, rounded to cents.
    double amount(double mu, double sigma) {
        double u1 = uniform(), u2 = uniform();
        double z = std::sqrt(-2.0 * std::log(1.0 - u1)) * std::cos(6.283185307179586 * u2);
        double a = std::exp(mu + sigma * z);
        if (a < 10.0) a = 10.0;
        return std::round(a * 100.0) / 100.0;
    }
};

struct Mix { const char* country; int weight; };   // residence mix per booking unit

const char* pick(Rng& r, const std::vector<Mix>& mix) {
    int total = 0;
    for (auto& m : mix) total += m.weight;
    int x = r.below(total);
    for (auto& m : mix) { if (x < m.weight) return m.country; x -= m.weight; }
    return mix.back().country;
}

}  // namespace

std::string lei_with_check_digits(const std::string& prefix18) {
    if (prefix18.size() != 18) throw std::invalid_argument("LEI prefix must have 18 characters");
    int mod = 0;
    for (char c : prefix18 + "00") {
        int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'Z') ? c - 'A' + 10 : -1;
        if (v < 0) throw std::invalid_argument("LEI prefix must be [A-Z0-9]");
        mod = (v >= 10) ? (mod * 100 + v) % 97 : (mod * 10 + v) % 97;
    }
    char cd[3];
    std::snprintf(cd, sizeof cd, "%02d", 98 - mod);
    return prefix18 + cd;
}

Book generate(const std::string& profile, const std::string& ref_date, std::uint64_t seed) {
    if (profile != "demo") throw std::invalid_argument("unknown entity profile: " + profile + " (known: demo)");
    Book b;
    b.entity_id = profile;
    b.ref_date = ref_date;
    b.seed = seed;
    // Demo entity: Belgian credit institution, EU parent in the Netherlands, a Luxembourg branch that
    // serves Germany and France cross-border, a French payment-institution subsidiary (K003).
    b.units.push_back({lei_with_check_digits("KYRA00DEMOBANKBE00"), "0400000042", "Kyra Demo Bank NV",
                       "Kyra Demo Bank NV", "Credit institution", "BELGIUM", "BE_NBB", "BE_FSMA", "", true});
    b.units.push_back({lei_with_check_digits("KYRA00DEMOBANKLU01"), "B000042", "Kyra Demo Bank NV Luxembourg branch",
                       "Kyra Demo Bank NV Luxembourg branch", "Credit institution", "LUXEMBOURG",
                       "LU - Commission de Surveillance du Secteur Financier", "LU-Commissariat aux Assurances",
                       "Branch", true});
    b.units.push_back({lei_with_check_digits("KYRA00DEMOPAIEFR02"), "17420", "Kyra Demo Paiements SAS",
                       "Kyra Demo Paiements SAS", "Payment institutions", "FRANCE",
                       "FR - Prudential Supervision and Resolution Authority", "FR-French Financial Markets Authority",
                       "Subsidiary", false});
    b.has_eu_parent = true;
    b.parent = {lei_with_check_digits("KYRA00DEMOHOLDNL00"), "34000042", "Kyra Demo Holding NV",
                "Kyra Demo Holding NV", "NETHERLANDS", true};

    const std::vector<std::vector<Mix>> mix = {
        {{"BELGIUM", 85}, {"NETHERLANDS", 8}, {"GERMANY", 7}},
        {{"LUXEMBOURG", 80}, {"GERMANY", 12}, {"FRANCE", 8}},
        {{"FRANCE", 100}},   // the subsidiary serves its home market only (fps = false)
    };
    const int per_unit[] = {1400, 400, 200};
    const char* products[] = {"current account", "savings", "consumer loan", "mortgage", "payment services"};
    const char* channels[] = {"branch", "online", "intermediary"};
    Rng r{seed};
    std::uint32_t id = 1;
    for (std::uint16_t u = 0; u < 3; ++u) {
        for (int i = 0; i < per_unit[u]; ++i, ++id) {
            Customer c;
            c.id = id;
            c.unit = u;
            c.residence = pick(r, mix[u]);
            int rc = r.below(100);
            c.risk = rc < 70 ? RiskClass::Low : rc < 95 ? RiskClass::Medium : RiskClass::High;
            c.product = products[r.below(5)];
            c.channel = channels[r.below(3)];
            c.pep = r.below(1000) < 4;
            int n = 1 + r.below(12);
            for (int t = 0; t < n; ++t) {
                bool in = r.below(2) == 0;
                b.transactions.push_back({c.id, in, r.amount(c.risk == RiskClass::High ? 8.5 : 7.0, 1.2),
                                          r.below(10) == 0 ? "UNITED STATES" : c.residence});
            }
            b.customers.push_back(std::move(c));
        }
    }
    return b;
}

}  // namespace kyra

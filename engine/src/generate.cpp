// Stage 1: synthetic AML book. Own PRNG (splitmix64) so the book is identical on every compiler.
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

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
    const bool third = profile == "demo_third";   // K023: head office outside the EU instead of an EU parent
    if (profile != "demo" && !third)
        throw std::invalid_argument("unknown entity profile: " + profile + " (known: demo, demo_third)");
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
    if (third) {
        b.head_office_outside_eu = true;
        b.parent = {lei_with_check_digits("KYRA00DEMOHOLDCH00"), "CHE000042", "Kyra Demo Holding AG",
                    "Kyra Demo Holding AG", "SWITZERLAND", true};
    } else {
        b.has_eu_parent = true;
        b.parent = {lei_with_check_digits("KYRA00DEMOHOLDNL00"), "34000042", "Kyra Demo Holding NV",
                    "Kyra Demo Holding NV", "NETHERLANDS", true};
    }

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
    enrich_aml(b, seed);
    return b;
}

namespace {

// k = max(1, round(rate * pool size)) distinct members, partial Fisher-Yates (quota, never zero; K013).
template <class F>
void quota(Rng& r, std::vector<Customer*> pool, double rate, F set) {
    if (pool.empty()) return;
    std::size_t k = std::size_t(std::llround(rate * double(pool.size())));
    if (k < 1) k = 1;
    if (k > pool.size()) k = pool.size();
    for (std::size_t i = 0; i < k; ++i) {
        std::size_t j = i + std::size_t(r.next() % std::uint64_t(pool.size() - i));
        std::swap(pool[i], pool[j]);
        set(*pool[i]);
    }
}

template <class P>
std::vector<Customer*> where(std::vector<Customer*>& all, P pred) {
    std::vector<Customer*> v;
    for (auto* c : all) if (pred(*c)) v.push_back(c);
    return v;
}

}  // namespace

// Second PRNG stream (K013): AML attributes, transaction kinds and activity volumes for aml_ra. Touches no field
// that aml_eligibility reads except residence of non-FPS home customers (moved to a non-EEA state, still non-FPS).
void enrich_aml(Book& b, std::uint64_t seed) {
    Rng r{seed ^ 0x4B5952415F414D4Cull};   // "KYRA_AML"
    std::vector<Customer*> ent;            // the legal entity: head office + branches (K004)
    for (auto& c : b.customers)
        if (c.unit == 0 || b.units[c.unit].structure == "Branch") ent.push_back(&c);
    auto all = ent;
    quota(r, all, 0.14, [](Customer& c) { c.aml.legal = true; });
    auto np = where(all, [](const Customer& c) { return !c.aml.legal; });
    auto le = where(all, [](const Customer& c) { return c.aml.legal; });
    // Non-EEA corporates booked at the head office (K016): home-resident, so never FPS customers.
    auto home_le = where(le, [&](const Customer& c) { return c.unit == 0 && c.residence == b.units[0].country; });
    const char* non_eea[] = {"SWITZERLAND", "UNITED KINGDOM", "UNITED STATES"};
    int ne = 0;
    quota(r, home_le, 0.03, [&](Customer& c) { c.residence = non_eea[ne++ % 3]; c.aml.professional = true; });
    if (where(np, [](const Customer& c) { return c.pep; }).empty()) quota(r, np, 0.004, [](Customer& c) { c.pep = true; });
    quota(r, le, 0.01, [](Customer& c) { c.aml.le_pep = true; });
    quota(r, all, 0.09, [](Customer& c) { c.aml.is_new = true; });
    auto fresh = where(all, [](const Customer& c) { return c.aml.is_new; });
    quota(r, fresh, 0.35, [](Customer& c) { c.aml.remote = true; });
    auto not_remote = where(fresh, [](const Customer& c) { return !c.aml.remote; });
    quota(r, not_remote, 0.15, [](Customer& c) { c.aml.third_party = true; });
    quota(r, where(fresh, [](const Customer& c) { return c.aml.third_party; }), 0.3,
          [](Customer& c) { c.aml.third_unsupervised = true; });
    quota(r, le, 0.08, [](Customer& c) { c.aml.complex = true; });
    quota(r, le, 0.03, [](Customer& c) { c.aml.bo_high_risk = true; });
    quota(r, all, 0.02, [](Customer& c) { c.aml.high_risk_activity = true; });
    quota(r, all, 0.005, [](Customer& c) { c.aml.fiu_request = true; });
    // W_07.03 four-class scale (K017): low stays low, medium splits 60/40 medium-low/medium-high, high stays high.
    for (auto* c : all)
        c->aml.risk4 = c->risk == RiskClass::Low ? 0 : c->risk == RiskClass::High ? 3 : (r.below(100) < 60 ? 1 : 2);
    quota(r, fresh, 0.03, [](Customer& c) { c.aml.no_profile = true; });   // onboarded, profile pending
    quota(r, le, 0.005, [](Customer& c) { c.aml.bo_not_identified = true; });
    quota(r, le, 0.015, [](Customer& c) { c.aml.bo_not_verified = true; });
    quota(r, all, 0.004, [](Customer& c) { c.aml.missing_docs = true; });
    quota(r, all, 0.008, [](Customer& c) { c.aml.cdd_gap = true; });
    quota(r, all, 0.12, [](Customer& c) { c.aml.review_due = true; });
    quota(r, where(all, [](const Customer& c) { return c.aml.review_due; }), 0.85,
          [](Customer& c) { c.aml.review_done = true; });
    // Products and services (K015).
    quota(r, le, 0.10, [](Customer& c) { c.aml.viban = true; });
    quota(r, np, 0.05, [](Customer& c) { c.aml.prepaid = true; });
    quota(r, where(np, [](const Customer& c) { return c.aml.prepaid; }), 0.04, [](Customer& c) { c.aml.multi_prepaid = true; });
    quota(r, le, 0.03, [&](Customer& c) { c.aml.factoring = true; c.aml.factoring_value = r.amount(12.4, 0.8); });
    quota(r, np, 0.06, [&](Customer& c) { c.aml.insured = true; c.aml.premium = r.amount(7.8, 0.6); });
    quota(r, np, 0.06, [](Customer& c) { c.aml.rto = true; });
    quota(r, le, 0.20, [](Customer& c) { c.aml.rto = true; c.aml.professional = true; });
    quota(r, np, 0.04, [](Customer& c) { c.aml.custody = true; });
    quota(r, le, 0.15, [](Customer& c) { c.aml.custody = true; c.aml.professional = true; });
    for (auto* c : all) if (c->aml.legal && c->residence != b.units[c->unit].country && c->aml.professional) {
        bool eea = c->residence != "SWITZERLAND" && c->residence != "UNITED KINGDOM" && c->residence != "UNITED STATES";
        if (!eea) c->aml.rto = c->aml.custody = true;
    }
    quota(r, np, 0.03, [&](Customer& c) { c.aml.portfolio = true; c.aml.aum = r.amount(12.4, 0.9); });
    quota(r, le, 0.05, [&](Customer& c) { c.aml.portfolio = true; c.aml.professional = true; c.aml.aum = r.amount(14.5, 0.9); });
    quota(r, where(le, [](const Customer& c) { return c.aml.portfolio; }), 0.05,
          [&](Customer& c) { c.aml.aum = 5.5e6 + r.amount(14.0, 0.5); });   // W_04.01 c0110, W_04.03 c0010
    quota(r, np, 0.01, [](Customer& c) { c.aml.private_banking = true; });
    quota(r, le, 0.01, [](Customer& c) { c.aml.tcsp = true; });
    quota(r, np, 0.02, [&](Customer& c) { c.aml.crypto = true; c.aml.crypto_value = r.amount(8.0, 1.0); });
    quota(r, np, 0.015, [](Customer& c) { c.aml.crypto_custody = true; });
    quota(r, np, 0.03, [](Customer& c) { c.aml.safe_box = true; });
    // W_04.11 investor buckets (K018): fund investors are a small subset of customers, about 5% of natural
    // persons as retail (UCITS/AIF 70/30) and a few legal entities as professional (UCITS/AIF 55/45).
    quota(r, np, 0.05, [&](Customer& c) { c.aml.fund_investor = true; c.aml.fund = r.below(100) < 70 ? 0 : 2; });
    quota(r, le, 0.03, [&](Customer& c) { c.aml.fund_investor = true; c.aml.fund = r.below(100) < 55 ? 1 : 3; });
    for (auto* c : all) {
        if (c->aml.fund_investor) c->aml.fund_flow = r.amount(7.6, 1.0);
        if (c->product == "consumer loan" || c->product == "mortgage") {
            c->aml.loan = c->product == "mortgage" ? r.amount(12.15, 0.5) : r.amount(9.4, 0.7);
            c->aml.loan_new = c->aml.is_new || r.below(100) < 12;
        }
    }
    auto loans = where(all, [](const Customer& c) { return c.aml.loan > 0; });
    quota(r, where(loans, [](const Customer& c) { return c.product == "mortgage"; }), 0.02,
          [](Customer& c) { c.aml.loan_third_party = true; });
    quota(r, loans, 0.01, [](Customer& c) { c.aml.loan_cash_collateral = true; });
    quota(r, loans, 0.03, [](Customer& c) { c.aml.loan_premature = true; });
    quota(r, where(loans, [](const Customer& c) { return c.product == "consumer loan"; }), 0.04,
          [](Customer& c) { c.aml.loan_unspecified = true; });

    // Transaction kinds (K014). Remittances go to remittance corridors, trade finance is for legal entities.
    std::vector<const Customer*> by_id(b.customers.size() + 1, nullptr);
    for (const auto& c : b.customers) by_id[c.id] = &c;
    const char* corridors[] = {"MOROCCO", "TURKEY", "ROMANIA", "POLAND", "PHILIPPINES"};
    for (auto& t : b.transactions) {
        const Customer& c = *by_id[t.customer];
        int x = r.below(100);
        if (x < 8) t.kind = TxKind::Cash;
        else if (x < 11 && !c.aml.legal) { t.kind = TxKind::Remittance; t.counterparty_country = corridors[r.below(5)]; }
        else if (x < 15 && c.aml.legal) t.kind = TxKind::TradeFinance;
        else if (x < 18) t.kind = TxKind::EMoney;
        else if (x < 20) t.kind = TxKind::CurrencyExchange;
    }

    // Activity volumes (K015), scaled to the entity's customer base.
    AmlActivity& a = b.activity;
    double n = double(all.size());
    a.respondents = {{"UNITED STATES", 2, r.amount(17.2, 0.3), r.amount(17.1, 0.3)},
                     {"SWITZERLAND", 1, r.amount(16.5, 0.3), r.amount(16.4, 0.3)},
                     {"TURKEY", 1, r.amount(15.8, 0.3), r.amount(15.9, 0.3)},
                     {"MOROCCO", 1, r.amount(15.1, 0.3), r.amount(15.2, 0.3)}};
    a.distribution = {{"BELGIUM", 12 + r.below(6), 3, 1, r.amount(13.2, 0.2)},
                      {"LUXEMBOURG", 2 + r.below(3), 1, 0, r.amount(11.9, 0.2)}};
    a.walk_in = int(n * 0.02) + 1;
    a.walk_in_transactions = a.walk_in * 2 + r.below(a.walk_in);
    a.reissued_vibans = 3 + r.below(10);
    a.bin_sponsored = 1 + r.below(3);
    a.open_funds = 4 + r.below(5);
    a.closed_funds = 1 + r.below(3);
    a.projects = 8 + r.below(10);
    a.donors_high_risk = 1 + r.below(4);
    a.projects_high_risk = 1;
    a.surrender_value = r.amount(13.0, 0.3);
    a.crowdfunding_value = r.amount(13.3, 0.4);
    a.ucits_aum = r.amount(19.5, 0.2);
    a.aif_aum = r.amount(18.6, 0.2);
    a.aif_unlisted = a.aif_aum * 0.35;
    double loans_out = 0;
    for (auto* c : all) loans_out += c->aml.loan;
    a.total_assets = std::round(loans_out / 0.62 * 100.0) / 100.0;   // loans ~62% of total assets (K019)
}

}  // namespace kyra

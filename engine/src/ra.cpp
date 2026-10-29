// Stage 2, aml_ra_v110: book -> FactsConverter facts, 34 templates (template -> source map in docs/ARCHITECTURE.md).
// Regkeys from M:/WIP/EBA/EBA 4.4 fase 1/aml_ra_v110/Regkey (tests/data/aml_ra_regkeys.txt); enumerations as the
// AGU_AML_RA_v110_i.xltx dropdown labels (K012). Rule ids in comments are the EBA 4.4 aml_ra validation rules.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>

#include "kyra/pipeline.hpp"

namespace kyra {
namespace {

std::string b2s(bool v) { return v ? "true" : "false"; }
std::string money(double v) {
    if (!(v > 0.004)) return "";               // no zeros as data: an absent amount stays blank
    char s[48];
    std::snprintf(s, sizeof s, "%.2f", v);
    return s;
}
std::string num(long long v) { return v > 0 ? std::to_string(v) : ""; }
std::string dec(double v, int digits) {
    char s[48];
    std::snprintf(s, sizeof s, "%.*f", digits, v);
    return s;
}

struct Out {
    Module& m;
    void cell(const char* sheet, const char* row, const char* col, const std::string& v, int ordinal = 0) {
        if (v.empty()) return;
        m.facts.push_back({sheet, std::string("T") + sheet + "-" + row + "-" + col, ordinal, v});
    }
    void c(const char* sheet, const char* col, const std::string& v) { cell(sheet, "L0010", col, v); }
};

bool non_eea(const std::string& country) {
    static const std::set<std::string> eea = {"AUSTRIA", "BELGIUM", "BULGARIA", "CROATIA", "CYPRUS", "CZECHIA",
        "DENMARK", "ESTONIA", "FINLAND", "FRANCE", "GERMANY", "GREECE", "HUNGARY", "IRELAND", "ITALY", "LATVIA",
        "LITHUANIA", "LUXEMBOURG", "MALTA", "NETHERLANDS", "POLAND", "PORTUGAL", "ROMANIA", "SLOVAKIA", "SLOVENIA",
        "SPAIN", "SWEDEN", "ICELAND", "LIECHTENSTEIN", "NORWAY"};
    return !eea.count(country);
}

struct Flow { long long n_in = 0, n_out = 0, big_in = 0, big_out = 0; double v_in = 0, v_out = 0; };
void add(Flow& f, const Transaction& t) {
    if (t.incoming) { ++f.n_in; f.v_in += t.amount_eur; f.big_in += t.amount_eur > 1000.0; }
    else { ++f.n_out; f.v_out += t.amount_eur; f.big_out += t.amount_eur > 1000.0; }
}

struct Geo {   // one W_05.01 row
    long long np = 0, le = 0, np_pep = 0, le_pep = 0, investors = 0, respondents = 0, branches = 0, subsidiaries = 0;
    Flow tx, remit;
    double ciu = 0, aum = 0, resp_in = 0, resp_out = 0;
};

}  // namespace

Module aggregate_ra(const Book& b) {
    if (b.units.empty()) throw std::invalid_argument("book has no reporting obliged entity");
    if (b.ref_date.size() != 10) throw std::invalid_argument("ref date must be YYYY-MM-DD");
    Module m{"aml_ra", "AML_RA", "AGU_AML_RA_v110_i.xltx", {}};
    Out o{m};
    const Unit& e = b.units[0];
    const AmlActivity& a = b.activity;
    const int year = std::stoi(b.ref_date.substr(0, 4));
    auto date = [](int y, int mo, int d) { char s[16]; std::snprintf(s, sizeof s, "%04d-%02d-%02d", y, mo, d); return std::string(s); };

    // Scope: the legal entity = head office + branches (K004); subsidiaries report their own aml_ra.
    std::vector<const Customer*> cs;
    std::map<std::uint32_t, const Customer*> by_id;
    for (const auto& c : b.customers)
        if (c.unit == 0 || b.units[c.unit].structure == "Branch") { cs.push_back(&c); by_id[c.id] = &c; }

    // W_01.01 basic information. The demo entity offers every product and service line (K020).
    o.c("W_01.01", "C0010", e.lei);
    o.c("W_01.01", "C0020", e.national_code);
    o.c("W_01.01", "C0030", e.name);
    o.c("W_01.01", "C0040", e.name_latin);
    o.c("W_01.01", "C0050", e.category);
    o.c("W_01.01", "C0060", "Credit institution with payment, investment and crypto-asset services");   // e904550_e_19
    o.c("W_01.01", "C0070", e.country);
    o.c("W_01.01", "C0080", "Euro");
    for (int col = 90; col <= 290; col += 10) {
        char k[8]; std::snprintf(k, sizeof k, "C%04d", col);
        o.c("W_01.01", k, "true");
    }

    // W_01.02 comment and W_01.03 not-applicable flags. PMR exports a row only with all three keys, country
    // included (TRAPS T8), so both explain blank cells of the geographic templates W_05.01 / W_06.01.
    o.cell("W_01.02", "L005", "C0010", "AML.06.01", 1);
    o.cell("W_01.02", "L005", "C0020", "C0050", 1);
    o.cell("W_01.02", "L005", "C0030", "LUXEMBOURG", 1);
    o.cell("W_01.02", "L005", "C0040", "No white labelling partners for the Luxembourg branch in the reference year", 1);
    o.cell("W_01.03", "L005", "C0010", "AML.06.01", 1);
    o.cell("W_01.03", "L005", "C0020", "C0050", 1);
    o.cell("W_01.03", "L005", "C0030", "LUXEMBOURG", 1);
    o.cell("W_01.03", "L005", "C0040", "Not applicable variable", 1);
    o.cell("W_01.03", "L005", "C0010", "AML.05.01", 2);   // respondents are third-country institutions only
    o.cell("W_01.03", "L005", "C0020", "C0130", 2);
    o.cell("W_01.03", "L005", "C0030", e.country, 2);
    o.cell("W_01.03", "L005", "C0040", "Not applicable variable", 2);

    // Customer counts.
    long long n = 0, np = 0, le = 0, np_pep = 0, le_pep = 0, active = 0, fresh = 0, remote = 0, third = 0, unsup = 0,
              complex = 0, hra = 0, bo_hr = 0, xb = 0, fiu = 0;
    long long risk4[4] = {0, 0, 0, 0}, no_prof = 0, bo_ni = 0, bo_nv = 0, docs = 0, cdd = 0, due = 0, done = 0;
    std::map<std::uint32_t, long long> tx_count;
    std::set<std::uint32_t> xb_customers;
    for (const auto& t : b.transactions)
        if (by_id.count(t.customer)) {
            ++tx_count[t.customer];
            if (non_eea(t.counterparty_country)) xb_customers.insert(t.customer);
        }
    for (const auto* c : cs) {
        const CustomerAml& x = c->aml;
        ++n; (x.legal ? le : np)++;
        np_pep += !x.legal && c->pep; le_pep += x.legal && x.le_pep;
        active += tx_count.count(c->id) > 0;
        fresh += x.is_new; remote += x.remote; third += x.third_party; unsup += x.third_unsupervised;
        complex += x.complex; hra += x.high_risk_activity; bo_hr += x.bo_high_risk; fiu += x.fiu_request;
        xb += xb_customers.count(c->id);
        if (!x.no_profile) ++risk4[x.risk4];
        no_prof += x.no_profile; bo_ni += x.bo_not_identified; bo_nv += x.bo_not_verified; docs += x.missing_docs;
        cdd += x.cdd_gap; due += x.review_due; done += x.review_done;
    }
    const char* w0201[] = {"C0010", "C0020", "C0030", "C0040", "C0050", "C0060", "C0070", "C0080", "C0090", "C0100",
                           "C0110", "C0120", "C0130", "C0140", "C0150", "C0160", "C0170"};
    long long v0201[] = {n, np, le, np_pep, le_pep, active, fresh, remote, third, unsup, complex, hra, bo_hr, xb,
                         a.walk_in, a.walk_in_transactions, fiu};
    for (int i = 0; i < 17; ++i) o.c("W_02.01", w0201[i], num(v0201[i]));

    // Transactions by kind.
    Flow pay, vib, fx, remit, trade, emoney, cash;
    std::map<std::uint32_t, double> cash_total;
    std::set<std::uint32_t> trade_customers;
    double fx_c2c = 0;
    long long outbound = 0;
    for (const auto& t : b.transactions) {
        auto it = by_id.find(t.customer);
        if (it == by_id.end()) continue;
        const Customer& c = *it->second;
        outbound += !t.incoming && t.kind != TxKind::Cash && t.kind != TxKind::CurrencyExchange;
        switch (t.kind) {
        case TxKind::Transfer:
            if (c.aml.viban) add(vib, t);
            if (c.product == "current account" || c.product == "payment services" || c.aml.viban) add(pay, t);
            break;
        case TxKind::Cash: add(cash, t); cash_total[t.customer] += t.amount_eur; break;
        case TxKind::Remittance: add(remit, t); break;
        case TxKind::TradeFinance: add(trade, t); trade_customers.insert(t.customer); break;
        case TxKind::EMoney: add(emoney, t); break;
        case TxKind::CurrencyExchange: add(fx, t); fx_c2c += t.amount_eur * 0.4; break;
        }
    }

    // W_03.01 payment accounts: current account / payment services customers and vIBAN masters (one account each).
    long long accounts = 0, masters = 0, prepaid = 0, multi = 0;
    for (const auto* c : cs) {
        accounts += c->product == "current account" || c->product == "payment services" || c->aml.viban;
        masters += c->aml.viban; prepaid += c->aml.prepaid; multi += c->aml.multi_prepaid;
    }
    o.c("W_03.01", "C0010", num(accounts));
    o.c("W_03.01", "C0020", money(pay.v_in)); o.c("W_03.01", "C0030", num(pay.n_in));
    o.c("W_03.01", "C0040", money(pay.v_out)); o.c("W_03.01", "C0050", num(pay.n_out));
    o.c("W_03.02", "C0010", num(masters));
    o.c("W_03.02", "C0020", num(vib.n_in)); o.c("W_03.02", "C0030", money(vib.v_in));
    o.c("W_03.02", "C0040", num(vib.n_out)); o.c("W_03.02", "C0050", money(vib.v_out));
    o.c("W_03.02", "C0060", num(a.reissued_vibans));
    // W_03.03 prepaid: one card per holder, four for holders with multiple cards; average load 240 EUR.
    long long cards = prepaid + 3 * multi;
    o.c("W_03.03", "C0010", num(cards));
    o.c("W_03.03", "C0020", money(cards * 240.0));
    o.c("W_03.03", "C0030", money(cards * 240.0 * 0.35));
    o.c("W_03.03", "C0040", num(prepaid));
    o.c("W_03.03", "C0050", num(multi));                       // v903801

    // W_03.04 lending and factoring.
    long long loans = 0, re = 0, re3 = 0, cashcol = 0, prem = 0, unspec = 0, fact = 0, nonfact = 0;
    double loan_v = 0, granted = 0, fact_v = 0, fact_ne = 0;
    for (const auto* c : cs) {
        const CustomerAml& x = c->aml;
        if (x.loan > 0) {
            ++loans; loan_v += x.loan; re += c->product == "mortgage"; re3 += x.loan_third_party;
            if (x.loan_new) granted += x.loan;
            cashcol += x.loan_cash_collateral; prem += x.loan_premature; unspec += x.loan_unspecified;
        }
        if (x.factoring) { fact += 2; fact_v += x.factoring_value; ++nonfact; }
    }
    fact_ne = fact_v * 0.12;                                    // non-EEA obligors share (K015)
    long long repay = loans * 12, repay_ne = std::max<long long>(1, std::llround(repay * 0.005));
    o.c("W_03.04", "C0010", num(loans)); o.c("W_03.04", "C0020", money(loan_v));
    o.c("W_03.04", "C0030", num(re)); o.c("W_03.04", "C0040", num(re3));
    o.c("W_03.04", "C0050", money(granted)); o.c("W_03.04", "C0060", num(cashcol));
    o.c("W_03.04", "C0070", num(repay)); o.c("W_03.04", "C0080", num(prem)); o.c("W_03.04", "C0090", num(repay_ne));
    o.c("W_03.04", "C0100", num(unspec)); o.c("W_03.04", "C0110", num(fact));
    o.c("W_03.04", "C0120", money(fact_v)); o.c("W_03.04", "C0130", money(fact_ne));

    // W_03.05 life insurance (bancassurance book of the entity).
    double gwp = 0; long long policies = 0;
    for (const auto* c : cs) if (c->aml.insured) { gwp += c->aml.premium; ++policies; }
    o.c("W_03.05", "C0010", money(gwp));
    o.c("W_03.05", "C0020", money(a.surrender_value));
    o.c("W_03.05", "C0030", "0.2200");                         // v903773, v903832: fraction 0..1
    o.c("W_03.05", "C0040", num(std::max<long long>(1, policies * 15 / 100)));

    // W_03.06 currency exchange involving cash: customer buys foreign currency = entity sells (outgoing).
    o.c("W_03.06", "C0010", num(fx.n_out)); o.c("W_03.06", "C0020", num(fx.n_in));
    o.c("W_03.06", "C0030", num(fx.big_out)); o.c("W_03.06", "C0040", num(fx.big_in));
    o.c("W_03.06", "C0050", money(fx.v_out)); o.c("W_03.06", "C0060", money(fx.v_in));
    o.c("W_03.06", "C0070", money(fx_c2c));                     // v904946

    // W_04.01 investment services (MiFID classification).
    long long rto_r = 0, rto_p = 0, rto_ne = 0, cus_r = 0, cus_p = 0, cus_ne = 0, pm_r = 0, pm_p = 0, big = 0, pb = 0;
    double aum = 0;
    for (const auto* c : cs) {
        const CustomerAml& x = c->aml;
        bool ne = non_eea(c->residence);
        if (x.rto) { (x.professional ? rto_p : rto_r)++; rto_ne += ne; }
        if (x.custody) { (x.professional ? cus_p : cus_r)++; cus_ne += ne; }
        if (x.portfolio) { (x.professional ? pm_p : pm_r)++; aum += x.aum; big += x.aum >= 5e6; }
        pb += x.private_banking;
    }
    o.c("W_04.01", "C0010", num(rto_r)); o.c("W_04.01", "C0020", num(rto_p)); o.c("W_04.01", "C0030", num(rto_ne));
    o.c("W_04.01", "C0040", num(cus_r)); o.c("W_04.01", "C0050", num(cus_p));
    o.c("W_04.01", "C0060", "0.3500");                         // v903787, v903833
    o.c("W_04.01", "C0070", num(cus_ne));
    o.c("W_04.01", "C0080", num(pm_r)); o.c("W_04.01", "C0090", num(pm_p));
    o.c("W_04.01", "C0100", money(aum)); o.c("W_04.01", "C0110", num(big));

    // W_04.02 money remittance (v903806).
    o.c("W_04.02", "C0010", num(remit.n_in)); o.c("W_04.02", "C0020", num(remit.n_out));
    o.c("W_04.02", "C0030", money(remit.v_in)); o.c("W_04.02", "C0040", money(remit.v_out));
    o.c("W_04.02", "C0050", num(remit.big_in)); o.c("W_04.02", "C0060", num(remit.big_out));
    o.c("W_04.03", "C0010", num(big)); o.c("W_04.03", "C0020", num(pb));

    // W_04.04 correspondent services; payable-through 8%, nested 5% of respondent flows (K015).
    double r_in = 0, r_out = 0;
    for (const auto& rsp : a.respondents) { r_in += rsp.in; r_out += rsp.out; }
    o.c("W_04.04", "C0010", money(r_in)); o.c("W_04.04", "C0020", money(r_out));
    o.c("W_04.04", "C0030", money(r_in * 0.08)); o.c("W_04.04", "C0040", money(r_out * 0.08));
    o.c("W_04.04", "C0050", money(r_in * 0.05)); o.c("W_04.04", "C0060", money(r_out * 0.05));

    o.c("W_04.05", "C0010", num(trade_customers.size()));
    o.c("W_04.05", "C0020", num(trade.n_in)); o.c("W_04.05", "C0030", num(trade.n_out));
    o.c("W_04.05", "C0040", money(trade.v_in)); o.c("W_04.05", "C0050", money(trade.v_out));
    o.c("W_04.06", "C0010", num(emoney.n_in)); o.c("W_04.06", "C0020", num(emoney.n_out));
    o.c("W_04.06", "C0030", money(emoney.v_in)); o.c("W_04.06", "C0040", money(emoney.v_out));
    o.c("W_04.06", "C0050", money((emoney.v_in + emoney.v_out) * a.emoney_unidentified_share));

    long long tcsp = 0, crypto = 0, ccust = 0, boxes = 0;
    double cvalue = 0;
    for (const auto* c : cs) {
        tcsp += c->aml.tcsp; boxes += c->aml.safe_box; ccust += c->aml.crypto_custody;
        if (c->aml.crypto) { ++crypto; cvalue += c->aml.crypto_value; }
    }
    o.c("W_04.07", "C0010", num(tcsp));                        // v903765
    o.c("W_04.08", "C0010", num(a.bin_sponsored));
    o.c("W_04.09", "C0010", num(ccust)); o.c("W_04.09", "C0020", money(cvalue * 0.6));

    // W_04.10 crypto exchange and transfer: per active crypto customer about six operations per flow (K015).
    long long f2c_n = crypto * 6, c2f_c = std::max<long long>(1, crypto * 7 / 10), c2f_n = c2f_c * 5;
    long long c2c_c = std::max<long long>(1, crypto * 4 / 10), c2c_n = c2c_c * 4;
    long long tr_c = std::max<long long>(1, crypto / 2), tr_n = tr_c * 3;
    auto part = [](long long total, int pct) { return std::max<long long>(1, total * pct / 100); };
    o.c("W_04.10", "C0010", money(cvalue)); o.c("W_04.10", "C0020", num(f2c_n)); o.c("W_04.10", "C0030", num(crypto));
    o.c("W_04.10", "C0040", num(part(f2c_n, 10)));
    o.c("W_04.10", "C0050", money(cvalue * 0.55)); o.c("W_04.10", "C0060", num(c2f_n)); o.c("W_04.10", "C0070", num(c2f_c));
    o.c("W_04.10", "C0080", num(part(c2f_n, 8)));
    o.c("W_04.10", "C0090", money(cvalue * 0.3)); o.c("W_04.10", "C0100", num(c2c_c)); o.c("W_04.10", "C0110", num(c2c_n));
    o.c("W_04.10", "C0120", num(part(c2c_n, 10))); o.c("W_04.10", "C0130", num(part(c2c_n, 8)));   // v904958
    o.c("W_04.10", "C0140", money(cvalue * 0.25)); o.c("W_04.10", "C0150", num(tr_c)); o.c("W_04.10", "C0160", num(tr_n));
    o.c("W_04.10", "C0170", num(part(tr_n, 12))); o.c("W_04.10", "C0180", num(part(tr_n, 9)));     // v904960

    // W_04.11 funds: investor buckets over all customers (K018, v905230 with v903846).
    long long fund[4] = {0, 0, 0, 0};
    for (const auto* c : cs) ++fund[c->aml.fund];
    o.c("W_04.11", "C0010", num(fund[0])); o.c("W_04.11", "C0020", num(fund[1]));
    o.c("W_04.11", "C0030", money(a.ucits_aum));
    o.c("W_04.11", "C0040", num(fund[2])); o.c("W_04.11", "C0050", num(fund[3]));
    o.c("W_04.11", "C0060", num(a.open_funds)); o.c("W_04.11", "C0070", num(a.closed_funds));
    o.c("W_04.11", "C0080", money(a.aif_aum)); o.c("W_04.11", "C0090", money(a.aif_unlisted));   // v904961
    o.c("W_04.12", "C0010", num(boxes));
    o.c("W_04.13", "C0010", money(a.crowdfunding_value)); o.c("W_04.13", "C0020", num(a.projects));
    o.c("W_04.13", "C0030", num(a.donors_high_risk)); o.c("W_04.13", "C0040", num(a.projects_high_risk));

    // W_04.14 cash: withdrawals = outgoing, deposits = incoming; customers above EUR 20 000 cumulated in the year.
    long long cash20 = 0;
    for (const auto& [id, v] : cash_total) cash20 += v > 20000.0;
    o.c("W_04.14", "C0010", num(cash.n_out)); o.c("W_04.14", "C0020", num(cash.n_in));
    o.c("W_04.14", "C0030", money(cash.v_out)); o.c("W_04.14", "C0040", money(cash.v_in));
    o.c("W_04.14", "C0050", num(cash20));

    // W_05.01 geographies: customers by residence, transactions by counterparty country (K021).
    std::map<std::string, Geo> geo;
    for (const auto* c : cs) {
        Geo& g = geo[c->residence];
        (c->aml.legal ? g.le : g.np)++;
        g.np_pep += !c->aml.legal && c->pep; g.le_pep += c->aml.legal && c->aml.le_pep;
        ++g.investors; g.ciu += c->aml.fund_flow; g.aum += c->aml.portfolio ? c->aml.aum : 0;
    }
    for (const auto& t : b.transactions) {
        if (!by_id.count(t.customer)) continue;
        Geo& g = geo[t.counterparty_country];
        add(g.tx, t);
        if (t.kind == TxKind::Remittance) add(g.remit, t);
    }
    for (const auto& rsp : a.respondents) { Geo& g = geo[rsp.country]; g.respondents += rsp.institutions; g.resp_in += rsp.in; g.resp_out += rsp.out; }
    for (std::size_t i = 1; i < b.units.size(); ++i)
        (b.units[i].structure == "Branch" ? geo[b.units[i].country].branches : geo[b.units[i].country].subsidiaries)++;
    int row = 0;
    for (const auto& [country, g] : geo) {
        ++row;
        const char* s = "W_05.01";
        o.cell(s, "L005", "C0010", country, row);
        o.cell(s, "L005", "C0020", num(g.np), row); o.cell(s, "L005", "C0030", num(g.le), row);
        o.cell(s, "L005", "C0040", num(g.np_pep), row); o.cell(s, "L005", "C0050", num(g.le_pep), row);
        o.cell(s, "L005", "C0060", num(g.tx.n_in), row); o.cell(s, "L005", "C0070", money(g.tx.v_in), row);
        o.cell(s, "L005", "C0080", num(g.tx.n_out), row); o.cell(s, "L005", "C0090", money(g.tx.v_out), row);
        o.cell(s, "L005", "C0100", money(g.ciu), row); o.cell(s, "L005", "C0110", num(g.investors), row);
        o.cell(s, "L005", "C0120", money(g.aum), row); o.cell(s, "L005", "C0130", num(g.respondents), row);
        o.cell(s, "L005", "C0140", money(g.resp_in), row); o.cell(s, "L005", "C0150", money(g.resp_out), row);
        o.cell(s, "L005", "C0160", num(g.branches), row); o.cell(s, "L005", "C0170", num(g.subsidiaries), row);
        o.cell(s, "L005", "C0180", money(g.remit.v_in), row); o.cell(s, "L005", "C0190", money(g.remit.v_out), row);
    }

    // W_06.01 distribution channels per country.
    row = 0;
    for (const auto& d : a.distribution) {
        ++row;
        o.cell("W_06.01", "L005", "C0010", d.country, row);
        o.cell("W_06.01", "L005", "C0020", num(d.agents), row);
        o.cell("W_06.01", "L005", "C0030", num(d.distributors), row);
        o.cell("W_06.01", "L005", "C0040", money(d.gwp_brokers), row);
        o.cell("W_06.01", "L005", "C0050", num(d.white_label), row);
    }

    // W_07.01-W_07.07 controls (K022). Dates fall before the reference date (v903766, v903791-v903800).
    o.c("W_07.01", "C0010", date(year, 3, 14));
    o.c("W_07.01", "C0020", dec(std::max(2.0, std::round(double(n) / 150.0 * 2.0) / 2.0), 1));
    o.c("W_07.01", "C0030", "1.0000"); o.c("W_07.01", "C0040", "0.9300");
    o.c("W_07.01", "C0050", "0.8100"); o.c("W_07.01", "C0060", "0.8900");
    o.c("W_07.02", "C0010", "Quarterly");
    const char* outsourcing[] = {"Not outsourced", "Partially outsourced", "Not outsourced", "Not outsourced",
                                 "Partially outsourced", "Partially outsourced", "Not outsourced", "Not outsourced",
                                 "Partially outsourced"};
    const char* c0702[] = {"C0020", "C0030", "C0040", "C0050", "C0060", "C0070", "C0080", "C0090", "C0100"};
    for (int i = 0; i < 9; ++i) o.c("W_07.02", c0702[i], outsourcing[i]);
    const char* audits[] = {"C0110", "C0120", "C0130", "C0140", "C0150", "C0160", "C0170", "C0180", "C0190"};
    const int audit_month[] = {11, 9, 6, 10, 4, 5, 2, 7, 3};
    for (int i = 0; i < 9; ++i) o.c("W_07.02", audits[i], date(year - 1, audit_month[i], 15));
    o.c("W_07.03", "C0010", date(year, 2, 27)); o.c("W_07.03", "C0020", "true"); o.c("W_07.03", "C0030", date(year, 1, 31));
    o.c("W_07.03", "C0040", num(risk4[0])); o.c("W_07.03", "C0050", num(risk4[1]));
    o.c("W_07.03", "C0060", num(risk4[2])); o.c("W_07.03", "C0070", num(risk4[3]));       // v903919
    o.c("W_07.04", "C0010", num(bo_ni)); o.c("W_07.04", "C0020", num(bo_nv)); o.c("W_07.04", "C0030", num(docs));
    o.c("W_07.04", "C0040", num(cdd)); o.c("W_07.04", "C0050", num(no_prof));
    o.c("W_07.04", "C0060", num(due)); o.c("W_07.04", "C0070", num(done));                // v904974

    // W_07.05: automated monitoring; alerts = transactions >= EUR 10 000, or >= EUR 3 000 for high-risk customers.
    long long alerts = 0;
    for (const auto& t : b.transactions) {
        auto it = by_id.find(t.customer);
        if (it != by_id.end() && (t.amount_eur >= 10000.0 || (it->second->risk == RiskClass::High && t.amount_eur >= 3000.0)))
            ++alerts;
    }
    alerts = std::max<long long>(alerts, 1);
    long long strs = std::max<long long>(1, alerts / 50);
    o.c("W_07.05", "C0010", "true");
    o.c("W_07.05", "C0020", "At least partially automated");   // c0030 blank: v903748
    for (const char* k : {"C0040", "C0050", "C0060", "C0070", "C0080"}) o.c("W_07.05", k, "true");
    o.c("W_07.05", "C0090", num(std::max<long long>(1, alerts * 4 / 100)));
    o.c("W_07.05", "C0100", "6.5");
    o.c("W_07.05", "C0110", num(alerts)); o.c("W_07.05", "C0120", num(strs));
    o.c("W_07.05", "C0130", "true"); o.c("W_07.05", "C0140", "11.5");
    o.c("W_07.05", "C0150", num(strs + fiu));
    o.c("W_07.06", "C0010", "4");
    o.c("W_07.06", "C0020", num(std::max<long long>(1, outbound * 3 / 1000)));
    o.c("W_07.06", "C0030", num(outbound));                                                // v904975
    o.c("W_07.06", "C0040", "0.0012");
    // W_07.07: group entities = branch + subsidiary; one of two reports on deficiencies (K022).
    o.c("W_07.07", "C0010", "1.0000"); o.c("W_07.07", "C0020", "1.0000"); o.c("W_07.07", "C0030", "1.0000");
    o.c("W_07.07", "C0040", "1.0000"); o.c("W_07.07", "C0050", "0.5000"); o.c("W_07.07", "C0060", "1.0000");
    o.c("W_07.07", "C0070", "1");   // c0080 blank: no group entity outside the EU/EEA

    o.cell("W_08.01", "L0380", "C0010", money(a.total_assets));
    return m;
}

}  // namespace kyra

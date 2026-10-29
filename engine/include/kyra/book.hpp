// Kyra book: the synthetic AML data of one reporting obliged entity (docs/ARCHITECTURE.md, data model).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kyra {

// One legal entity or establishment; index 0 of Book::units is the reporting obliged entity itself.
struct Unit {
    std::string lei, national_code, name, name_latin;
    std::string category;          // [Type of AML entity] label, e.g. "Credit institution"
    std::string country;           // [Location of activity] label, e.g. "BELGIUM"
    std::string supervisor;        // [AML Financial Supervisor] label
    std::string add_supervisor;    // [Additional AML Financial Supervisor] label
    std::string structure;         // [Group structure] label for establishments ("Branch", "Subsidiary"); "" for the entity
    bool fps = false;              // operates through freedom to provide services (K004)
};

struct Parent {                    // EU parent undertaking (V_02.01)
    std::string lei, national_code, name, name_latin, country;
    bool aml_supervised = true;
};

enum class RiskClass : std::uint8_t { Low, Medium, High };

// AML attributes of a customer for aml_ra (K013-K016). Drawn from a second PRNG stream after the
// eligibility book, so aml_eligibility output does not move. Flags are quotas (>= 1 customer each).
struct CustomerAml {
    bool legal = false;            // legal entity (else natural person)
    bool le_pep = false;           // legal entity with a PEP, family member or close associate as BO
    bool is_new = false, remote = false, third_party = false, third_unsupervised = false;
    bool complex = false, high_risk_activity = false, bo_high_risk = false, fiu_request = false;
    std::uint8_t risk4 = 0;        // W_07.03: 0 low, 1 medium-low, 2 medium-high, 3 high
    bool no_profile = false, bo_not_identified = false, bo_not_verified = false, missing_docs = false;
    bool cdd_gap = false, review_due = false, review_done = false;
    bool viban = false, prepaid = false, multi_prepaid = false, factoring = false, insured = false;
    bool rto = false, custody = false, portfolio = false, professional = false, private_banking = false;
    bool tcsp = false, crypto = false, crypto_custody = false, safe_box = false;
    std::uint8_t fund = 0;         // W_04.11 investor bucket: 0 UCITS retail, 1 UCITS prof, 2 AIF retail, 3 AIF prof
    double loan = 0;               // outstanding loan (consumer loan / mortgage products)
    bool loan_new = false, loan_third_party = false, loan_cash_collateral = false, loan_premature = false;
    bool loan_unspecified = false;
    double aum = 0, fund_flow = 0, premium = 0, crypto_value = 0, factoring_value = 0;
};

struct Customer {
    std::uint32_t id = 0;
    std::uint16_t unit = 0;        // booking unit (index into Book::units)
    std::string residence;         // country label
    RiskClass risk = RiskClass::Low;
    std::string product;           // product risk factor (K005)
    std::string channel;           // delivery channel risk factor (K005)
    bool pep = false;
    CustomerAml aml;
};

// Transaction kind (K014): drives W_03.01, W_03.02, W_03.06, W_04.02, W_04.05, W_04.06, W_04.14.
enum class TxKind : std::uint8_t { Transfer, Cash, Remittance, TradeFinance, EMoney, CurrencyExchange };

struct Transaction {
    std::uint32_t customer = 0;
    bool incoming = true;
    double amount_eur = 0;         // > 0, cents
    std::string counterparty_country;
    TxKind kind = TxKind::Transfer;
};

struct Respondent { std::string country; int institutions = 0; double in = 0, out = 0; };     // W_04.04, W_05.01
struct Distribution { std::string country; int agents = 0, distributors = 0, white_label = 0; double gwp_brokers = 0; };

// Activity volumes without a customer-level model yet (K015): drawn once per book, scaled to the customer base.
struct AmlActivity {
    std::vector<Respondent> respondents;
    std::vector<Distribution> distribution;
    int walk_in = 0, walk_in_transactions = 0, reissued_vibans = 0, bin_sponsored = 0;
    int open_funds = 0, closed_funds = 0, projects = 0, donors_high_risk = 0, projects_high_risk = 0;
    double surrender_value = 0, crowdfunding_value = 0;
    double ucits_aum = 0, aif_aum = 0, aif_unlisted = 0, emoney_unidentified_share = 0.02;
    double total_assets = 0;
};

struct Book {
    std::string entity_id;         // demo profile name
    std::string ref_date;          // YYYY-MM-DD
    std::uint64_t seed = 0;
    std::vector<Unit> units;
    bool has_eu_parent = false, head_office_outside_eu = false;
    Parent parent;
    std::vector<Customer> customers;
    std::vector<Transaction> transactions;
    AmlActivity activity;
};

}  // namespace kyra

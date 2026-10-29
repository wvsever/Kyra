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

struct Customer {
    std::uint32_t id = 0;
    std::uint16_t unit = 0;        // booking unit (index into Book::units)
    std::string residence;         // country label
    RiskClass risk = RiskClass::Low;
    std::string product;           // product risk factor (K005)
    std::string channel;           // delivery channel risk factor (K005)
    bool pep = false;
};

struct Transaction {
    std::uint32_t customer = 0;
    bool incoming = true;
    double amount_eur = 0;         // > 0, cents
    std::string counterparty_country;
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
};

}  // namespace kyra

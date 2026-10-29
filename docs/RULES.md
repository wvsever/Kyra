# Kyra rules

- Synthetic data only. Kyra never reads customer data and never goes online.
- Production code is C++20; Python only for tests.
- No zeros as data: a cell is a real value or blank by rule (K007).
- Each entity profile fits its business model; no service line the entity would not offer.
- Only PMR and Yena measure assertions (K010). Never adapt a validation rule; report a broken one.
- Push only through `lt q submit` (pre-push guard); never push by hand, never rewrite main.
- Decisions: one K line in docs/DECISIONS.md with the regulatory source; mark what is assumed.
- Docs are two-tier: CLAUDE.md/README.md short, detail here in docs/. Plain text, fewest words.
- Plans carry phases and exit criteria, never dates.

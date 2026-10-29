// product_version.h -- product version API (LaneTools T-spec "product versioning", identical in every repo).
// Declarations only: the SHA and dates live in one generated .cpp (<build>/<product>_version_info.cpp),
// compiled into a tiny static library linked into the exe, so no other TU recompiles or misses the
// compiler cache when HEAD changes. Do not add a macro or constant with the SHA to any header.
#pragma once

namespace product_version {

const char* product();     // "elara"
const char* version();     // YY.M.D from the HEAD commit date, like Yena (26.9.23)
const char* sha();         // short git SHA (9 chars) or "unknown"
bool dirty();              // tracked files modified at generation time
const char* build_date();  // YYYY-MM-DD (UTC) of the generation for this HEAD
const char* line();        // "elara 26.10.28 sha=0123abcde dirty=0 built=2026-10-28"
const char* json();        // {"product":"elara","version":"26.10.28","sha":"0123abcde","dirty":false,"build_date":"2026-10-28"}

// arg = argv[1]. Prints line() for --version, json() for --version-json, then returns 0; else -1.
int handle(const char* arg);

}  // namespace product_version

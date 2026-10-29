#!/usr/bin/env python3
"""PreToolUse guard for unattended (bypassPermissions) Claude Code runs.

Permission deny rules match a command by PREFIX only, so they cannot express
"sqlcmd is fine but not when the batch contains UPDATE". This hook sees the
whole command string and can.

Reads the PreToolUse payload on stdin, denies on a match, stays silent otherwise.
Exit code is always 0; the decision travels in the JSON on stdout.
"""
import json
import re
import sys


def deny(reason):
    json.dump({
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": reason,
        }
    }, sys.stdout)
    sys.exit(0)


# (compiled pattern, reason). First match wins.
RULES = []


def rule(pattern, reason):
    RULES.append((re.compile(pattern, re.IGNORECASE), reason))


# --- Secret files: .env / .env.* / *.pfx / .credentials.json -----------------
# These used to be permissions.deny rules (Read(**/.env) and friends). They were
# moved here on 2026-09-04: from claude.exe 2.1.258 the mere PRESENCE of a
# Read() deny rule arms a static path analyser over EVERY Bash command
# (oZe() -> bashMissKind "cd-compound-read", "flag-validation", ...). Anything
# it cannot resolve statically -- a relative read after a cd in a compound
# command, a flagged cp/mv, a pipeline -- becomes an ASK, and an unattended
# lane cannot answer an ask. A hook only ever returns deny, so it never asks.
#
# The command form is deliberately narrower than a bare filename match: many
# C# projects legitimately NAME a .pfx on a build line. Only a command that
# would READ or COPY the content is blocked.
SECRET_FILE = re.compile(
    r"(^|[\\/])\.env(\.[^\\/]*)?$"
    r"|\.pfx$"
    r"|(^|[\\/])\.credentials\.json$",
    re.IGNORECASE,
)
SECRET_IN_COMMAND = re.compile(
    r"(^|[\\/\s\"'=])\.env(\.[A-Za-z0-9_.-]+)?($|[\s\"';|&)])"
    r"|\.pfx($|[\s\"';|&)])"
    r"|(^|[\\/\s\"'=])\.credentials\.json($|[\s\"';|&)])",
    re.IGNORECASE,
)
SECRET_READER = re.compile(
    r"\b(cat|type|head|tail|less|more|strings|base64|xxd|od|hexdump|"
    r"grep|rg|egrep|fgrep|sed|awk|cut|nl|tee|"
    r"Get-Content|gc|Select-String|sls|Format-Hex|"
    r"cp|copy|Copy-Item|scp|curl|Invoke-WebRequest|Invoke-RestMethod)\b",
    re.IGNORECASE,
)

SECRET_REASON = (
    "BLOCKED by overnight_guard: that command would read or copy a secret file "
    "(.env, .env.*, *.pfx or .credentials.json). Standing user rule: agents never "
    "read credential material. This replaces the permissions.deny Read(**/.env) "
    "rules, which were removed because their mere presence made Claude Code ASK "
    "on ordinary Bash reads and broke unattended runs. If you need to know a "
    "setting's NAME, list the keys without the values."
)


def check_secrets(cmd):
    if SECRET_READER.search(cmd) and SECRET_IN_COMMAND.search(cmd):
        deny(SECRET_REASON)


# --- Bacco / XbrlFactoryMaster: SELECT + read-only metadata only ------------
# Agents may AUTHOR DDL/DML for the owner to run; they may never EXECUTE it.
SQL_CLIENT = re.compile(r"\b(sqlcmd|invoke-sqlcmd|bcp|osql)\b", re.IGNORECASE)
SQL_WRITE = re.compile(
    # \b, not [\s;(] -- the statement usually sits right after -Q " or -Query ".
    r"\b("
    r"insert\s+into|update\s+\w|delete\s+from|merge\s+into|truncate\s+table|"
    r"drop\s+(table|view|proc|procedure|function|index|database|schema)|"
    r"alter\s+(table|view|proc|procedure|function|index|database|schema)|"
    r"create\s+(table|view|proc|procedure|function|index|database|schema)|"
    r"grant\s|revoke\s|bulk\s+insert|backup\s+database|restore\s+database|"
    r"exec\s+\w|execute\s+\w|sp_executesql|select\s.*\sinto\s+\w"
    r")",
    re.IGNORECASE | re.DOTALL,
)
# BaselCalculator is the one writable DB on that server.
WRITABLE_DB = re.compile(r"baselcalculator", re.IGNORECASE)
# Deliberately excludes the server hostname "bacco" -- it appears in every -S
# argument, so including it would make the BaselCalculator exemption dead code.
READONLY_DB = re.compile(
    r"xbrlfactory|xbrl_staging|xbrl_dwh|fgxrxbrl", re.IGNORECASE
)


def check_sql(cmd):
    if not SQL_CLIENT.search(cmd) or not SQL_WRITE.search(cmd):
        return
    # Allow writes that clearly target BaselCalculator and nothing read-only.
    if WRITABLE_DB.search(cmd) and not READONLY_DB.search(cmd):
        return
    deny(
        "BLOCKED by overnight_guard: this looks like SQL DML/DDL executed against a "
        "Bacco database (XbrlFactoryMaster / XBRL_STAGING_* / XBRL_DWH_* / FGXRXBRL_*). "
        "Standing user rule: agents NEVER execute writes there -- only SELECT and "
        "read-only metadata (sys.*, OBJECT_DEFINITION, INFORMATION_SCHEMA, sp_help). "
        "Write the SQL to a file under F:\\Temp for the owner to review and run, and "
        "report it as a finding instead."
    )


# --- Deployed service: killing it is never the fix --------------------------
rule(
    r"(stop-process|taskkill|pkill|kill\s+-9).{0,120}?bcal_service",
    "BLOCKED by overnight_guard: never kill bcal_service. A DEPLOYED WildFly instance "
    "auto-restarts and writes the same DB, so this reaps a live run without stopping "
    "the writer. Filter on the worktree path and stop that process instead.",
)

# --- Mirror copies wipe the destination -------------------------------------
rule(
    r"\brobocopy\b.*(/MIR|/PURGE)",
    "BLOCKED by overnight_guard: robocopy /MIR and /PURGE delete everything in the "
    "destination that is absent from the source. This is blocked for F: space "
    "reclaim -- use Directory.Move staging + attrib -R + Directory.Delete.",
)

# --- Shared-checkout build script -------------------------------------------
rule(
    r"build_yena\.ps1",
    "BLOCKED by overnight_guard: tools\\build_yena.ps1 is hardcoded to the SHARED "
    "checkout and deploys over testcaserunner\\yena.exe. Build a worktree with "
    "F:\\Temp\\yenaflake\\build_lane.ps1 -Wt <wt> instead.",
)

# --- Branch-switching the shared main checkouts -----------------------------
# Caught in two shapes: "git -C <path> <op>" and "cd <path> ... git <op>".
REPOS = r"(?:baselcalculator|cppbankgenerator|cppbankrawaccgen|report)"
# The lookbehind pins the name to a whole path component, so a directory merely
# ENDING in one of these (F:\Temp\myreport, bcal_report) does not trip the
# guard -- "report" is short enough to fire on real work otherwise. The trailing
# group accepts a path INTO the repo (report\src), not just its root.
REPO_PATH = rf"(?<![\w.-]){REPOS}(?:[/\\]\S*)?"
GIT_WRITE = r"(?:switch|checkout|reset|rebase|commit|merge|cherry-pick|stash)"
CHDIR = r"(?:cd|chdir|pushd|set-location|push-location)"
SHARED_CHECKOUT_REASON = (
    "BLOCKED by overnight_guard: that is a SHARED main checkout other agents -- and "
    "for report/, another TEAM -- read as a stable reference; git allows one branch "
    "per working tree, so switching it breaks their in-flight builds. Use a worktree."
)

# "git -C <path> <op>". \S*? cannot cross a space, so a repo name appearing
# later in the line (e.g. inside -m "fix report parser") is unreachable here.
rule(
    rf"\bgit\s+(?:-C\s+)?[\"']?\S*?{REPO_PATH}[\"']?\s+{GIT_WRITE}\b",
    SHARED_CHECKOUT_REASON,
)
# "cd <repo> && git <op>". The tempered dot lets other commands sit between the
# cd and the git, but NOT another cd -- so "cd report; cd ../lane && git commit"
# stays allowed, the git op having left the shared checkout by then.
rule(
    # The lookahead makes the cd argument END at the repo path. Without it the
    # tempered dot below happily swallows the rest of a LONGER directory name,
    # and "cd F:\Temp\report_backup && git commit" is denied on the strength of
    # its first six letters.
    rf"(?s)\b{CHDIR}\s+[\"']?\S*?{REPO_PATH}[\"']?(?=[\s;&|]|$)"
    rf"(?:(?!\b{CHDIR}\b).){{0,300}}?"
    rf"\bgit\s+{GIT_WRITE}\b",
    SHARED_CHECKOUT_REASON,
)

# --- Force push --------------------------------------------------------------
rule(
    r"git\s+push\b.*(--force(?!-with-lease)|\s-f\b)",
    "BLOCKED by overnight_guard: force push. Push a lane with "
    "'git push origin <lane>:main' from the worktree instead.",
)

# --- Recursive deletion of a drive root -------------------------------------
rule(
    r"(rm\s+-[a-z]*[rR][a-z]*f|rm\s+-[a-z]*f[a-z]*[rR])\s+[\"']?"
    r"([a-zA-Z]:[/\\]?|/[a-z]|//[a-z])[\"']?\s*$",
    "BLOCKED by overnight_guard: recursive delete targeting a drive root.",
)
rule(
    r"(remove-item|rd|rmdir)\b.*\s[\"']?[a-zA-Z]:[\\/][\"']?(\s|$).*(-recurse|/s\b)",
    "BLOCKED by overnight_guard: recursive delete targeting a drive root.",
)


def main():
    try:
        payload = json.load(sys.stdin)
    except Exception:
        sys.exit(0)  # Unparseable input must not block the run.

    tool = payload.get("tool_name", "")
    tool_input = payload.get("tool_input") or {}

    if tool in ("Read", "Edit", "Write", "MultiEdit", "NotebookEdit"):
        for key in ("file_path", "notebook_path"):
            path = tool_input.get(key)
            if isinstance(path, str) and SECRET_FILE.search(path.replace("\\", "/")):
                deny(SECRET_REASON)
        sys.exit(0)

    if tool not in ("Bash", "PowerShell"):
        sys.exit(0)

    cmd = tool_input.get("command") or ""
    if not cmd:
        sys.exit(0)

    check_secrets(cmd)
    check_sql(cmd)
    for pattern, reason in RULES:
        if pattern.search(cmd):
            deny(reason)

    sys.exit(0)


if __name__ == "__main__":
    main()

// =============================================================================
// EXAMPLE MADE BY CLAUDE - NO AI WAS USED IN THE HEADER ITSELF!
// example.cpp - a guided tour of INIRW (inirw.h)
//
// Layout this file expects:
//     project/
//       src/inirw/inirw.h
//       examples/example.cpp      <- this file
//       examples/test.ini         <- sample input (never modified by this program)
//
// Build & run (from the examples/ folder, so "test.ini" is found):
//     g++ -std=c++17 -Wall -Wextra -o example example.cpp
//     ./example
//
// The program is self-checking: every claim it makes is verified with expect(),
// and the exit code is non-zero if any check fails.
// =============================================================================

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "./../src/inirw/inirw.h"

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Small helpers (these are NOT part of the library)
// -----------------------------------------------------------------------------

static int g_failed = 0;

static void banner(const std::string& title) {
    std::cout << "\n==== " << title << " ====\n";
}

// Prints [ OK ] / [FAIL] so the example doubles as a smoke test.
static void expect(bool condition, const std::string& what) {
    std::cout << (condition ? "  [ OK ] " : "  [FAIL] ") << what << "\n";
    if (!condition) ++g_failed;
}

// Pretty-print a whole Ini. Values are wrapped in '' so whitespace is visible.
static void dump(const INIRW::Ini& ini) {
    for (const auto& [section, keys] : ini) {
        std::cout << "  [" << (section.empty() ? "(global)" : section) << "]\n";
        for (const auto& [key, value] : keys)
            std::cout << "      " << key << " = '" << value << "'\n";
    }
}

// Print a file with line numbers so you can see exactly what INIRW wrote.
static void printFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string line;
    int n = 1;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::cout << "  " << std::setw(3) << n++ << " | " << line << "\n";
    }
}

static std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// Deep comparison of two Ini objects (same sections, same keys, same values).
static bool sameIni(const INIRW::Ini& a, const INIRW::Ini& b) {
    if (a.size() != b.size()) return false;
    for (const auto& [name, sect] : a) {
        auto other = b.find(name);
        if (other == b.end() || other->second.size() != sect.size()) return false;
        for (const auto& [key, value] : sect) {
            auto ov = other->second.find(key);
            if (ov == other->second.end() || ov->second != value) return false;
        }
    }
    return true;
}

// Typed, read-only lookups with a fallback. They use find(), so unlike
// operator[] they never create anything as a side effect.
static std::string getString(const INIRW::Ini& ini, const std::string& section,
                             const std::string& key, const std::string& fallback = "") {
    auto s = ini.find(section);
    if (s == ini.end()) return fallback;
    auto k = s->second.find(key);
    return k == s->second.end() ? fallback : k->second;
}

static int getInt(const INIRW::Ini& ini, const std::string& section,
                  const std::string& key, int fallback) {
    try { return std::stoi(getString(ini, section, key)); }
    catch (const std::exception&) { return fallback; }
}

static double getDouble(const INIRW::Ini& ini, const std::string& section,
                        const std::string& key, double fallback) {
    try { return std::stod(getString(ini, section, key)); }
    catch (const std::exception&) { return fallback; }
}

static bool getBool(const INIRW::Ini& ini, const std::string& section,
                    const std::string& key, bool fallback) {
    std::string v = getString(ini, section, key, fallback ? "true" : "false");
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return v == "true" || v == "yes" || v == "on" || v == "1";
}

int main() {
    const std::string sample = "test.ini";           // pristine input, never written
    const std::string work   = "test_modified.ini";  // working copy we patch below

    // =========================================================================
    banner("1. Reading a file");
    // =========================================================================
    INIRW::Ini ini;
    INIRW::read(sample, ini);   // read() clears `ini` first, then fills it

    std::cout << "  " << ini.size() << " sections, in file order "
                 "(the global section is stored under the name \"\"):\n";
    dump(ini);

    // =========================================================================
    banner("2. Looking things up safely");
    // =========================================================================
    expect(ini.contains("server"),   "contains(\"server\") is true");
    expect(!ini.contains("Server"),  "section names are case-sensitive");
    expect(!ini["server"].contains("Port"), "key names are case-sensitive too");

    std::string host   = getString(ini, "server", "host", "localhost");
    int         port   = getInt(ini, "server", "port", 80);
    double      tmo    = getDouble(ini, "server", "timeout", 1.0);
    bool        debug  = getBool(ini, "server", "debug", false);
    int         retry  = getInt(ini, "server", "retries", 3);   // key does not exist
    std::cout << "  host=" << host << " port=" << port << " timeout=" << tmo
              << " debug=" << std::boolalpha << debug << " retries(default)=" << retry << "\n";
    expect(host == "127.0.0.1", "inline comment was stripped from the value");
    expect(port == 8080 && tmo == 2.5 && debug, "typed helpers convert the strings");
    expect(retry == 3, "missing key -> fallback");
    expect(getString(ini, "", "app_name") == "INIRW Demo", "global keys live in section \"\"");

    // find() returns an iterator: ->first is the name, ->second is the value.
    auto sec = ini.find("list");
    if (sec != ini.end()) {
        std::cout << "  [" << sec->first << "] has " << sec->second.size() << " keys\n";
    }

    // Beware: operator[] creates missing entries (exactly like std::map).
    {
        INIRW::Ini probe;
        probe["a"]["b"] = "c";
        (void)probe["typo"]["x"];     // "just looking" ... but it creates [typo] and x
        expect(probe.size() == 2 && probe["typo"].contains("x"),
               "operator[] inserts when you only meant to look (use find/contains instead)");
    }

    // =========================================================================
    banner("3. Comments, escapes and unusual syntax");
    // =========================================================================
    expect(ini["server"]["motd"] == "Hello; world",         "\\;  -> literal semicolon in a value");
    expect(ini["database"]["password"] == "p#ssw;rd",       "\\# and \\; -> literal # and ;");
    expect(ini["paths"]["home"] == "C:\\Users\\demo",       "\\\\ -> one backslash (Windows paths must be doubled)");
    expect(ini["database"].contains("empty_value") &&
           ini["database"]["empty_value"].empty(),          "\"key =\" -> empty value");
    expect(ini["database"].contains("flag_without_equals") &&
           ini["database"]["flag_without_equals"].empty(),  "a line with no '=' -> key with empty value");
    expect(ini.contains("spaced section"),                  "whitespace inside [ ] is trimmed");
    expect(ini["spaced section"]["indented key"] == "lots of   space",
           "outer whitespace trimmed, inner whitespace kept");
    expect(ini.contains("special]chars"),                   "\\] allows ']' inside a section name");
    expect(ini["special]chars"].contains("key=with=equals"),"\\= allows '=' inside a key");
    expect(ini["special]chars"].contains("[bracketed]"),    "\\[ and \\] allow brackets inside a key");
    expect(ini["duplicates"].size() == 1 && ini["duplicates"]["color"] == "blue",
           "duplicate keys: last value wins, first position is kept");

    // =========================================================================
    banner("4. oMap: the insertion-ordered container behind Ini");
    // =========================================================================
    {
        INIRW::oMap<std::string, int> scores;
        scores["zeta"]  = 1;
        scores["alpha"] = 2;
        scores["mid"]   = 3;
        scores["alpha"] = 20;       // overwriting keeps the original position

        std::string order;
        for (const auto& [k, v] : scores) order += k + " ";
        expect(order == "zeta alpha mid ",
               "iteration = insertion order (std::map would sort, unordered_map would shuffle)");

        auto it = scores.find("mid");
        expect(it != scores.end() && it->second == 3,  "find() returns an iterator ...");
        expect(scores.find("nope") == scores.end(),    "... or end() when the key is absent");

        scores.erase("zeta");
        scores.erase("not there");  // erasing a missing key is a harmless no-op
        expect(!scores.contains("zeta") && scores.size() == 2, "erase() removes one entry");

        // Copies are deep and carry their own index.
        INIRW::oMap<std::string, int> copy = scores;
        copy["alpha"] = -1;
        copy["extra"] = 7;
        expect(scores["alpha"] == 20 && !scores.contains("extra"), "copies are independent");

        // Moves are cheap and keep the internal index valid.
        INIRW::oMap<std::string, int> moved = std::move(copy);
        expect(moved.size() == 3 && moved["extra"] == 7, "moved-to map is fully usable");

        // std::list underneath => references stay valid while you keep inserting.
        int& anchor = scores["mid"];
        for (int i = 0; i < 1000; ++i) scores["bulk" + std::to_string(i)] = i;
        anchor = 99;
        expect(scores["mid"] == 99, "references survive 1000 more insertions");

        scores.clear();
        expect(scores.empty() && scores.size() == 0, "clear() empties it");
    }

    // =========================================================================
    banner("5. Writing: INIRW patches the existing file instead of regenerating it");
    // =========================================================================
    fs::copy_file(sample, work, fs::copy_options::overwrite_existing);

    INIRW::Ini edit;
    INIRW::read(work, edit);

    edit["server"]["port"]    = "9090";      // change a value (its inline comment stays)
    edit["server"]["workers"] = "4";         // add a key to an existing section
    edit[""]["build"]         = "release";   // add a global key
    edit["cache"]["size"]     = "256";       // brand-new section, appended at the end
    edit["cache"]["ttl"]      = "60";
    edit["database"].erase("password");      // delete a key
    edit.erase("obsolete");                  // delete a whole section
    edit["database"]["flag_without_equals"] = "now_has_a_value";  // "key" -> "key = value"
    for (auto& [k, v] : edit["list"]) v += "!";                    // bulk edit via iteration
    edit["special]chars"].clear();           // remove all keys; the header line stays

    INIRW::write(work, edit);
    std::cout << "  Resulting " << work << " (compare with: diff test.ini " << work << "):\n";
    printFile(work);

    INIRW::Ini after;
    INIRW::read(work, after);
    expect(after["server"]["port"] == "9090",              "changed value was written");
    expect(after["server"]["workers"] == "4",              "new key in an existing section");
    expect(after[""]["build"] == "release",                "new global key");
    expect(after.contains("cache") && after["cache"]["ttl"] == "60", "new section");
    expect(!after["database"].contains("password"),        "deleted key is gone");
    expect(!after.contains("obsolete"),                    "deleted section is gone");
    expect(after["list"]["item2"] == "banana!",            "bulk edit via iteration");
    expect(after.contains("special]chars") && after["special]chars"].empty(),
           "cleared section keeps its header");
    expect(after["duplicates"].size() == 1 && after["duplicates"]["color"] == "blue",
           "duplicate keys collapse into one line on write");

    const std::string text = slurp(work);
    expect(text.find("; inline comment (stripped when reading)") != std::string::npos,
           "inline comment on an UNCHANGED line survived");
    expect(text.find("; ---- Plain keys, inline comments, typed values ----") != std::string::npos,
           "standalone comments survived");
    expect(text.find("[ spaced section ]") != std::string::npos &&
           text.find("    indented key    =    lots of   space") != std::string::npos,
           "your hand-made spacing survived");

    // =========================================================================
    banner("6. Creating a file from scratch + lossless round trip of tricky text");
    // =========================================================================
    fs::remove("generated.ini");                      // make sure the file really is new
    INIRW::Ini fresh;
    fresh[""]["title"]          = "global key";
    fresh["net"]["url"]         = "http://example.com/?a=1;b=2#frag";
    fresh["net"]["path"]        = "C:\\Program Files\\App";
    fresh["we[ird] sec;tion#"]["k=ey;#[]"] = "v;a#l=ue\\";
    INIRW::write("generated.ini", fresh);
    printFile("generated.ini");

    INIRW::Ini back;
    INIRW::read("generated.ini", back);
    expect(sameIni(fresh, back), "read(write(x)) == x, even with ; # = [ ] and backslashes");

    // =========================================================================
    banner("7. Byte-level fidelity: BOM, CRLF, no-op writes, atomic replace");
    // =========================================================================
    {   // 7a. UTF-8 BOM + Windows line endings
        {
            std::ofstream out("crlf_bom.ini", std::ios::binary | std::ios::trunc);
            out << "\xEF\xBB\xBF";
            out << "; windows style file\r\n[win]\r\nkey = old\r\n";
        }
        INIRW::Ini w;
        INIRW::read("crlf_bom.ini", w);
        expect(w.contains("win") && w["win"]["key"] == "old", "BOM and \\r never leak into names/values");

        w["win"]["key"]   = "new";
        w["win"]["added"] = "yes";
        INIRW::write("crlf_bom.ini", w);

        const std::string raw = slurp("crlf_bom.ini");
        std::size_t lf = std::count(raw.begin(), raw.end(), '\n');
        std::size_t crlf = 0;
        for (std::size_t p = 0; (p = raw.find("\r\n", p)) != std::string::npos; p += 2) ++crlf;
        expect(raw.compare(0, 3, "\xEF\xBB\xBF") == 0, "BOM is kept");
        expect(lf > 0 && lf == crlf,                   "every line ending is still CRLF (new lines too)");
        expect(!fs::exists("crlf_bom.ini.tmp"),        "no temp file left behind (temp file + rename)");
    }
    {   // 7b. A write with unchanged data leaves a hand-formatted file byte-identical
        const std::string tidy =
            "# my hand-formatted file\n"
            "\n"
            "top = 1\n"
            "\n"
            "[a]\n"
            "   x    =   10   ; keep my spacing\n"
            "y=20\n"
            "\n"
            "; comment before b\n"
            "[b]\n"
            "z = 30\n";
        { std::ofstream out("tidy.ini", std::ios::binary | std::ios::trunc); out << tidy; }
        INIRW::Ini t;
        INIRW::read("tidy.ini", t);
        INIRW::write("tidy.ini", t);
        expect(slurp("tidy.ini") == tidy, "no-op write is byte-for-byte identical");
    }

    // =========================================================================
    banner("8. Edge cases worth knowing about");
    // =========================================================================
    {
        std::cout << "  (the next two lines on stderr are expected)\n";
        INIRW::Ini nothing;
        INIRW::read("does_not_exist.ini", nothing);
        expect(nothing.empty(), "reading a missing file gives an empty Ini (plus a message on cerr)");

        INIRW::write("never_created.ini", nothing);
        expect(!fs::exists("never_created.ini"), "writing an EMPTY Ini is refused (it cannot be used to wipe a file)");

        fs::remove("ws.ini");
        INIRW::Ini ws;
        ws["s"]["padded"] = "  keep me  ";
        INIRW::write("ws.ini", ws);
        INIRW::Ini ws2;
        INIRW::read("ws.ini", ws2);
        expect(ws2["s"]["padded"] == "keep me", "leading/trailing spaces in values are trimmed on read");
    }

    // ---- tidy up scratch files (test_modified.ini and generated.ini are kept to inspect) ----
    for (const char* f : {"crlf_bom.ini", "tidy.ini", "ws.ini"}) fs::remove(f);

    std::cout << "\n" << (g_failed == 0 ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED")
              << " (" << g_failed << " failed)\n";
    return g_failed == 0 ? 0 : 1;
}

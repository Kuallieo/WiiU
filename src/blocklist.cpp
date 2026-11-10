#include "blocklist.h"

#include <nn/acp/client.h>
#include <nn/acp/title.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>
#include <memory>
#include <sys/stat.h>

namespace blocklist {
namespace {

// Starting point only - adjust the SD card file to your own collection.
// Names are matched case-insensitively as a part of the game's title (English, German, French, ... names).
constexpr const char *kDefaultFile =
        "# GamePad-Waechter - Sperrliste\n"
        "#\n"
        "# Spiele in dieser Liste werden im Modus \"Ohne GamePad\" nicht gestartet.\n"
        "# Eine Zeile pro Spiel. Erlaubt sind:\n"
        "#   - ein (Teil des) Spielnamens, Gross-/Kleinschreibung egal, z.B.  Wii Sports Club\n"
        "#   - eine Title-ID mit 16 Hex-Zeichen, z.B.  00050000-10101E00\n"
        "# Zeilen mit # am Anfang werden ignoriert.\n"
        "#\n"
        "# Diese Startliste ist nicht vollstaendig und nicht fuer jedes Spiel geprueft -\n"
        "# passe sie an deine Sammlung an.\n"
        "\n"
        "Wii Sports Club\n"
        "Nintendo Land\n"
        "Game & Wario\n"
        "ZombiU\n"
        "Wii Party U\n"
        "Wii Fit U\n"
        "Splatoon\n"
        "Star Fox Zero\n"
        "Star Fox Guard\n"
        "Kirby and the Rainbow\n"
        "Regenbogen-Pinsel\n"
        "Captain Toad\n"
        "Paper Mario: Color Splash\n"
        "Super Mario Maker\n"
        "Art Academy\n"
        "Affordable Space Adventures\n";

std::string Trim(const std::string &s) {
    size_t start = 0;
    size_t end   = s.size();
    while (start < end && std::isspace(static_cast<unsigned char>(s[start]))) start++;
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
    return s.substr(start, end - start);
}

// Lower-case (ASCII only) and collapse all whitespace (incl. line breaks in long names) to single spaces.
std::string Normalize(const char *s) {
    std::string out;
    bool lastWasSpace = true;
    for (; *s; s++) {
        auto c = static_cast<unsigned char>(*s);
        if (std::isspace(c)) {
            if (!lastWasSpace) out.push_back(' ');
            lastWasSpace = true;
            continue;
        }
        out.push_back(static_cast<char>(c < 0x80 ? std::tolower(c) : c));
        lastWasSpace = false;
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

bool ParseTitleId(const std::string &line, uint64_t &out) {
    std::string hex;
    for (char c : line) {
        if (c == '-' || c == ' ') continue;
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        hex.push_back(c);
    }
    if (hex.size() != 16) return false;
    out = std::strtoull(hex.c_str(), nullptr, 16);
    return out != 0;
}

void ParseLine(const std::string &raw, List &list) {
    std::string line = Trim(raw);
    if (line.empty() || line[0] == '#') return;
    // Allow trailing comments: "00050000-10101E00   # Nintendo Land"
    if (auto pos = line.find(" #"); pos != std::string::npos) line = Trim(line.substr(0, pos));
    if (line.empty()) return;

    Entry e;
    e.name = line;
    if (!ParseTitleId(line, e.titleId)) {
        e.titleId    = 0;
        e.normalized = Normalize(line.c_str());
        if (e.normalized.empty()) return;
    }
    list.entries.push_back(std::move(e));
}

void ParseText(const char *text, List &list) {
    std::string cur;
    for (const char *p = text; *p; p++) {
        if (*p == '\n') {
            ParseLine(cur, list);
            cur.clear();
        } else {
            cur.push_back(*p);
        }
    }
    ParseLine(cur, list);
}

struct MetaXmlDeleter {
    void operator()(ACPMetaXml *p) const { free(p); }
};

} // namespace

List Load() {
    List list;
    FILE *f = fopen(kFilePath, "r");
    if (!f) {
        ParseText(kDefaultFile, list);
        return list;
    }
    list.fromFile = true;
    char buf[512];
    std::string pending;
    while (fgets(buf, sizeof(buf), f)) {
        pending += buf;
        if (!pending.empty() && pending.back() == '\n') {
            ParseLine(pending, list);
            pending.clear();
        }
    }
    ParseLine(pending, list);
    fclose(f);
    return list;
}

void WriteDefaultFileIfMissing() {
    struct stat st {};
    if (stat(kFilePath, &st) == 0) return;
    mkdir("fs:/vol/external01/wiiu", 0777);
    mkdir(kDirPath, 0777);
    FILE *f = fopen(kFilePath, "w");
    if (!f) return;
    fputs(kDefaultFile, f);
    fclose(f);
}

bool Matches(const List &list, uint64_t titleId, std::string &outDisplayName) {
    // 1) Title ids - cheap, no system call needed.
    for (const auto &e : list.entries) {
        if (e.titleId != 0 && e.titleId == titleId) {
            outDisplayName = e.name;
            break;
        }
    }

    // 2) Read the game's names from its meta.xml (also gives us a nice display name).
    auto *raw = static_cast<ACPMetaXml *>(memalign(0x40, sizeof(ACPMetaXml)));
    if (!raw) return !outDisplayName.empty();
    std::unique_ptr<ACPMetaXml, MetaXmlDeleter> meta(raw);
    memset(raw, 0, sizeof(ACPMetaXml));

    ACPInitialize();
    ACPResult res = ACPGetTitleMetaXml(titleId, raw);
    ACPFinalize();
    if (res != ACP_RESULT_SUCCESS) return !outDisplayName.empty();

    // Make sure all strings are terminated, whatever the file contained.
    const char *fields[] = {
            meta->shortname_de, meta->shortname_en, meta->longname_de, meta->longname_en,
            meta->shortname_fr, meta->shortname_it, meta->shortname_es, meta->shortname_nl,
            meta->shortname_pt, meta->longname_fr, meta->longname_it, meta->longname_es,
            meta->longname_nl, meta->longname_pt};
    meta->shortname_de[sizeof(meta->shortname_de) - 1] = '\0';
    meta->shortname_en[sizeof(meta->shortname_en) - 1] = '\0';
    meta->shortname_fr[sizeof(meta->shortname_fr) - 1] = '\0';
    meta->shortname_it[sizeof(meta->shortname_it) - 1] = '\0';
    meta->shortname_es[sizeof(meta->shortname_es) - 1] = '\0';
    meta->shortname_nl[sizeof(meta->shortname_nl) - 1] = '\0';
    meta->shortname_pt[sizeof(meta->shortname_pt) - 1] = '\0';
    meta->longname_de[sizeof(meta->longname_de) - 1]   = '\0';
    meta->longname_en[sizeof(meta->longname_en) - 1]   = '\0';
    meta->longname_fr[sizeof(meta->longname_fr) - 1]   = '\0';
    meta->longname_it[sizeof(meta->longname_it) - 1]   = '\0';
    meta->longname_es[sizeof(meta->longname_es) - 1]   = '\0';
    meta->longname_nl[sizeof(meta->longname_nl) - 1]   = '\0';
    meta->longname_pt[sizeof(meta->longname_pt) - 1]   = '\0';

    const char *prettyName = meta->shortname_de[0] ? meta->shortname_de : meta->shortname_en;
    if (!outDisplayName.empty()) {
        // Matched by title id - prefer the real game name for the message.
        if (prettyName[0]) outDisplayName = prettyName;
        return true;
    }

    std::vector<std::string> names;
    for (const char *f : fields) {
        if (f[0]) names.push_back(Normalize(f));
    }

    for (const auto &e : list.entries) {
        if (e.titleId != 0) continue;
        for (const auto &n : names) {
            if (n.find(e.normalized) != std::string::npos) {
                outDisplayName = prettyName[0] ? prettyName : e.name;
                return true;
            }
        }
    }
    return false;
}

} // namespace blocklist

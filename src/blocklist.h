#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace blocklist {

// Location of the user-editable list on the SD card.
inline constexpr const char *kDirPath  = "fs:/vol/external01/wiiu/gamepad-guard";
inline constexpr const char *kFilePath = "fs:/vol/external01/wiiu/gamepad-guard/blocklist.txt";

struct Entry {
    // Either a full 64-bit title id (titleId != 0) or a name fragment.
    uint64_t titleId = 0;
    std::string name;       // as written in the file (for display)
    std::string normalized; // lower-case, single spaces (for matching)
};

struct List {
    std::vector<Entry> entries;
    bool fromFile = false;
};

// Loads the list from the SD card. Falls back to the built-in defaults if the file does not exist.
List Load();

// Creates the SD card file with the built-in defaults if it does not exist yet.
void WriteDefaultFileIfMissing();

// Returns true if the title matches an entry. outDisplayName receives a human-readable game name.
bool Matches(const List &list, uint64_t titleId, std::string &outDisplayName);

} // namespace blocklist

#pragma once
#include <string>

// Structured outcome of an AI chat turn. Callers must react to these fields,
// never by searching for magic substrings inside `reply`.
struct AiChatResult {
    enum class ModeChange { None, Auto, Manual };

    std::string reply;                        // text to show the user
    ModeChange modeChange = ModeChange::None; // AI asked to switch operation mode
    bool settingsApplied = false;             // AI changed freq/volt on the miner
};

#pragma once
#include "core/BitaxeData.h"
#include "core/OperationMode.h"
#include <string>
#include <cstddef>

// Builds the JSON body for the Web UI's GET /api/status (#70) from the same
// MainScreenSnapshot-shaped data the CYD's Main screen renders — no separate
// data path, so the web dashboard can't drift from what the physical screen
// shows. Pure/testable; the HTTP glue lives in main.cpp.
class StatusJsonBuilder {
public:
    static std::string build(const BitaxeData& data, OperationMode mode, bool wifiOk,
                              const float* tempHistory, size_t tempHistoryCount,
                              const float* hashHistory, size_t hashHistoryCount);
};

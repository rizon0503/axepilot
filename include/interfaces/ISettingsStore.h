#pragma once
#include "core/OperationMode.h"

// Persistent user settings that must survive a reboot / power loss.
class ISettingsStore {
public:
    virtual ~ISettingsStore() = default;
    virtual OperationMode loadMode(OperationMode fallback) = 0;
    virtual void saveMode(OperationMode mode) = 0;
};

#pragma once
#include "interfaces/ISettingsStore.h"

class EspSettingsStore : public ISettingsStore {
public:
    OperationMode loadMode(OperationMode fallback) override;
    void saveMode(OperationMode mode) override;
};

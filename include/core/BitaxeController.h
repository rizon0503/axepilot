#pragma once
#include "interfaces/IHttpClient.h"
#include "interfaces/ISystemTime.h"
#include "BitaxeData.h"
#include "InterventionLog.h"
#include <string>

class BitaxeController {
public:
    BitaxeController(IHttpClient& httpClient, ISystemTime& sysTime, const std::string& ipAddress);
    void update();
    BitaxeData getData() const;
    // Single point through which ALL frequency/voltage changes go to the
    // miner; `source` names the initiator for the intervention journal.
    void applySettings(int frequency, int coreVoltage, const char* source);
    void setFanSpeed(int percent); // manual fan, disables autofanspeed
    void setAutoFan();
    void restartMiner();
    const InterventionLog& interventions() const;

private:
    IHttpClient& httpClient;
    ISystemTime& sysTime;
    std::string ipAddress;
    BitaxeData currentData;
    uint32_t lastUpdate;
    InterventionLog interventionLog;
};

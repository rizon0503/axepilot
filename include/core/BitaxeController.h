#pragma once
#include "interfaces/IHttpClient.h"
#include "interfaces/ISystemTime.h"
#include "BitaxeData.h"
#include "InterventionLog.h"
#include <string>

class BitaxeController {
public:
    BitaxeController(IHttpClient& httpClient, ISystemTime& sysTime, const std::string& ipAddress);
    // Updates the address every subsequent call uses — lets the caller
    // resolve the Bitaxe via mDNS after construction instead of only ever
    // using the static IP passed to the constructor (#7).
    void setIpAddress(const std::string& ipAddress);
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

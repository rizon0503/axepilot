#include "core/InterventionLog.h"
#include <cstdio>
#include <cstring>
#include <ctime>

void InterventionLog::add(uint32_t nowMs, time_t epochSec, const char* source, int frequency, int coreVoltage) {
    Entry& entry = entries[head];
    entry.timeMs = nowMs;
    entry.epochSec = epochSec;
    snprintf(entry.source, sizeof(entry.source), "%s", source ? source : "?");
    entry.frequency = frequency;
    entry.coreVoltage = coreVoltage;

    head = (head + 1) % CAPACITY;
    if (count < CAPACITY) {
        count++;
    }
    total++;
}

size_t InterventionLog::size() const {
    return count;
}

uint32_t InterventionLog::totalCount() const {
    return total;
}

void InterventionLog::format(char* buf, size_t bufLen, uint32_t nowMs) const {
    if (bufLen == 0) return;
    buf[0] = '\0';
    size_t used = 0;

    for (size_t i = 0; i < count && used + 1 < bufLen; i++) {
        const Entry& e = entries[(head + CAPACITY - 1 - i) % CAPACITY]; // newest first

        char when[24];
        if (e.epochSec > 0) {
            time_t t = e.epochSec;
            struct tm* tmInfo = gmtime(&t);
            strftime(when, sizeof(when), "%H:%M UTC", tmInfo);
        } else {
            uint32_t agoSec = (nowMs - e.timeMs) / 1000;
            if (agoSec < 60) {
                snprintf(when, sizeof(when), "%us ago", (unsigned)agoSec);
            } else if (agoSec < 3600) {
                snprintf(when, sizeof(when), "%um ago", (unsigned)(agoSec / 60));
            } else {
                snprintf(when, sizeof(when), "%uh %um ago", (unsigned)(agoSec / 3600), (unsigned)((agoSec % 3600) / 60));
            }
        }

        int written = snprintf(buf + used, bufLen - used, "%s — %s: %d MHz / %d mV\n",
                               when, e.source, e.frequency, e.coreVoltage);
        if (written <= 0 || (size_t)written >= bufLen - used) {
            break; // buffer full
        }
        used += written;
    }
}

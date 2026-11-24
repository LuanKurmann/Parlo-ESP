#include "battery.h"

#define XPOWERS_CHIP_AXP2101
#include <Wire.h>
#include <XPowersLib.h>

#include "board.h"

namespace device {

namespace {
XPowersPMU pmu;
bool pmu_ok = false;
} // namespace

bool batteryBegin() {
    pmu_ok = pmu.begin(Wire, board::kPmuAddress, board::kI2cSda, board::kI2cScl);
    if (pmu_ok) {
        // Messungen einschalten, wie im Beispiel des Herstellers.
        pmu.enableBattDetection();
        pmu.enableBattVoltageMeasure();
        pmu.enableVbusVoltageMeasure();
    }
    return pmu_ok;
}

golf::BatteryInfo batteryRead() {
    golf::BatteryInfo info;
    if (!pmu_ok || !pmu.isBatteryConnect()) return info;
    info.known = true;
    info.percent = pmu.getBatteryPercent();
    info.charging = pmu.isCharging();
    return info;
}

} // namespace device

#include "Configuration.h"
#include "Datastore.h"
#include "Hoymiles.h"
#include "ModbusSunSpec.h"
#include "MessageOutput.h"
#include "SunPosition.h"

#include <string>
#include <algorithm>

ModbusSunSpecClass ModbusSunSpec;

ModbusSunSpecClass::ModbusSunSpecClass()
    : _loopTask(100 * TASK_MILLISECOND, TASK_FOREVER, std::bind(&ModbusSunSpecClass::loop, this))
    , _netTask(TASK_IMMEDIATE, TASK_FOREVER, std::bind(&ModbusSunSpecClass::task, this))
{
}

bool ModbusSunSpecClass::addHregS16(uint16_t offset, int16_t value) {
    return addHreg(offset, value);
}

bool ModbusSunSpecClass::addHregS32(uint16_t offset, int32_t value) {
    bool rc{true};
    rc &= addHreg(offset + 1, (value >> 16) & 0xFFFF);
    rc &= addHreg(offset + 0, value & 0xFFFF);

    return rc;
}
bool ModbusSunSpecClass::addHregU16(uint16_t offset, uint16_t value) {
    return addHreg(offset, value);
}

bool ModbusSunSpecClass::addHregU32(uint16_t offset, uint32_t value) {
    bool rc{true};
    rc &= addHreg(offset + 0, (value >> 16) & 0xFFFF);
    rc &= addHreg(offset + 1, value & 0xFFFF);

    return rc;
}

bool ModbusSunSpecClass::addHregStr(uint16_t offset, const String& value, uint16_t size) {
    addHreg(offset, 0, size / 2);
    return HregStr(offset, value, size);
}

bool ModbusSunSpecClass::HregS16(uint16_t offset, int16_t value) {
    return Hreg(offset, value);
}

bool ModbusSunSpecClass::HregU16(uint16_t offset, uint16_t value) {
    return Hreg(offset, value);
}

bool ModbusSunSpecClass::HregS32(uint16_t offset, int32_t value) {
    bool rc{true};
    rc &= Hreg(offset + 0, (value >> 16) & 0xFFFF);
    rc &= Hreg(offset + 1, value & 0xFFFF);

    return rc;
}

bool ModbusSunSpecClass::HregStr(uint16_t offset, const String& value, uint16_t size) {
    String tValue = value.substring(0, size);

    bool rc{true};
    const uint16_t* c = reinterpret_cast<const uint16_t*>(tValue.c_str());

    int i = 0;
    for(i = 0; i < (tValue.length() + 1) / 2; i++) {
        uint16_t v = (*c >> 8) & 0xFF;
        v |= (*c << 8) & 0xFF00;
        rc &= HregU16(offset + i, v);
        c++;
    }

    for(; i < size/2; i++) {
        HregU16(offset + i, 0);
    }

    return rc;
}

void ModbusSunSpecClass::init(Scheduler& scheduler) {
    const CONFIG_T& config = Configuration.get();

    if(!config.SunSpec.Enabled) {
        return;
    }

    server();

    if(config.SunSpec.RemoteControl) {
        MessageOutput.printf("Total Max Power: %uW\r\n", getTotalMaxPower());
        MessageOutput.printf("AC Output: %u phases\r\n", getPhaseCount());
    }

    // SolarEdge SunSpec Header
    addHregStr(40000, "SunS", 4);                                   // "SunS" SunSpec Identifier

    // Common Block 1
    addHregU16(40002, 0x01);                                        // SunSpec Common Block
    addHregU16(40003, 65);                                          // Block Length
    addHreg(40004, 0, 65);                                          // Reserve Block Data
    HregStr(40004, config.SunSpec.Manufacturer, SUNSPEC_MAX_MANUFACTURER_STRLEN);   // Manufacturer
    HregStr(40020, config.SunSpec.Model, SUNSPEC_MAX_MODEL_STRLEN);                 // Model
    HregStr(40044, std::to_string(config.Cfg.Version).c_str(), 16); // Version
    HregStr(40052, std::to_string(config.Dtu.Serial).c_str(), 32);  // Serial
    HregU16(40068, 126);                                            // Device Address

    auto block = 100 + getPhaseCount();

    // Phase Block
    addHregU16(40069, block);   // Phase Configuration
    addHregU16(40070, 50);      // Block Length
    addHreg(40071, 0, 50);      // Reserve Block Data
    HregU16(40071, 0);          // AC Total Current A
    HregU16(40072, 0);          // AC L1 Current A
    HregU16(40073, 0);          // AC L2 Current A
    HregU16(40074, 0);          // AC L3 Current A
    HregS16(40075, -2);         // AC Current Scale Factor
    HregU16(40079, 0);          // AC Voltage AN
    HregU16(40080, 0);          // AC Voltage BN
    HregU16(40081, 0);          // AC Voltage CN
    HregS16(40082, -1);         // AC Voltage Scale Factor
    HregS16(40083, 0);          // AC Total Power
    HregS16(40084, -1);         // AC Total Power Scale Factor
    HregU16(40085, 0);          // AC Frequency
    HregS16(40086, -1);         // AC Frequency Scale Factor
    HregS16(40087, 0);          // AC Apparent Power
    HregS16(40088, -1);         // AC Apparent Power Factor
    HregS16(40089, 0);          // AC Reactive Power
    HregS16(40090, -1);         // AC Reactive Power Factor
    HregS16(40091, 0);          // AC Power Factor
    HregS16(40092, -2);         // AC Power Factor Factor
    HregS32(40093, 0);          // AC Total Energy
    HregS16(40095, -1);         // AC Total Energy Factor
    HregU16(40096, 0);          // DC Current
    HregS16(40097, -2);         // DC Current Factor
    HregU16(40098, 0);          // DC Voltage
    HregS16(40099, -1);         // DC Voltage Factor
    HregS16(40100, 0);          // DC Power
    HregS16(40101, -1);         // DC Power Factor
    HregS16(40102, 0);          // Cabinet Temperature
    HregS16(40103, 0);          // Heat Sink Temperature
    HregS16(40104, 0);          // Transformer Temperature
    HregS16(40105, 0);          // Other Temperature
    HregS16(40106, -1);         // Temperature Factor
    HregU16(40107, 3);          // Status
    HregU16(40108, 0);          // Vendor Status

    // Nameplate Ratings Block
    addHregU16(40121, 120);             // Block ID
    addHregU16(40122, 26);              // Block Length
    addHreg(40123, 0, 26);              // Reserve Block Data
    HregU16(40123, 4);                  // DERTyp 4
    HregU16(40124, getTotalMaxPower()); // WRtg
    HregU16(40143, 0);                  // AhrRtg

    // Immediate controls
    addHregU16(40149, 123);  // Block ID
    addHregU16(40150, 24);   // Block Length
    addHreg(40151, 0, 24);   // Reserve Block Data
    HregU16(40151, 0);       // Conn_WinTms (unused)
    HregU16(40152, 0);       // Conn_RvrtTms (unused)
    HregU16(40153, 1);       // Conn (unused)
    HregU16(40154, 100);     // WMaxLimPct
    HregU16(40155, 0);       // WMaxLimPct_WinTms (unused)
    HregU16(40156, 0);       // WMaxLimPct_RvrtTms
    HregU16(40157, 0);       // WMaxLimPct_RmpTms (unused)
    HregU16(40158, 0);       // WMaxLim_Ena
    HregU16(40172, 0);       // WMaxLimPct_SF
    HregU16(40173, 0);       // OutPFSet_SF
    HregU16(40174, 0);       // VArPct_SF

    // Callbacks
    onSetHreg(40154, [this](TRegister* reg, uint16_t val) -> uint16_t {
        const CONFIG_T& config = Configuration.get();

        if(!config.SunSpec.RemoteControl) {
            return val;
        }

        static uint16_t limit_pct{100};
        static uint16_t limit_timeout{120};
        static bool limit_enable{false};

        bool changed{false};

        switch(reg->address.address) {
            case 40154:
                MessageOutput.printf("modbus: limit_pct = %u\r\n", val);
                changed = (limit_pct != val) && limit_enable;
                limit_pct = val;
                break;
            case 40156:
                MessageOutput.printf("modbus: limit_timeout = %u\r\n", val);
                changed = (limit_timeout != val) && limit_enable;
                limit_timeout = val;
                break;
            case 40158:
                MessageOutput.printf("modbus: limit_enable = %u\r\n", val);
                changed = (limit_enable != (val == 1));
                limit_enable = (val == 1);
                break;
        }

        if(changed) {
            this->setPowerLimit(limit_enable ? limit_pct : 100, limit_timeout);
        }

        return val;
    }, 5);

    // Block 0xFFFF
    addHregU16(40175, 65535);   // End Block Identifier
    addHregU16(40176, 0);       // Block Length

    scheduler.addTask(_loopTask);
    _loopTask.enable();

    scheduler.addTask(_netTask);
    _netTask.enable();
}

void ModbusSunSpecClass::loopPowerLimit() {
   int32_t now = (int32_t)millis();

    for (auto &limit : _limit) {
        if(limit.serial == 0) {
            continue;
        }

        auto conf = getConfig(limit.serial);
        if(conf == nullptr) {
            limit.serial = 0;
            continue;
        }

        if(limit.pending) {
            limit.pending = !setLimit(limit.serial, limit.power, "update");
        }

        if (now - limit.timeout > 0) {
            if(!limit.pending) {
                limit.serial = 0;
            }
            else {
                limit.power = conf->MaxPower;
                limit.pending = true;
            }
        }
    }
}

void ModbusSunSpecClass::task() {
    ModbusTCP::task();
}

void ModbusSunSpecClass::loop() {
    const CONFIG_T& config = Configuration.get();
    bool allProducing = Datastore.getIsAllEnabledProducing();

    // Status
    if(!SunPosition.isDayPeriod()) {
        HregU16(40107, 2);
    }
    else if(allProducing) {
        bool throttled = (Hreg(40158) == 1) && (Hreg(40154) < 100);
        HregU16(40107, throttled ? 5 : 4);
    }
    else if(Datastore.getIsAtLeastOneReachable()) {
        HregU16(40107, 3);
    }
    else {
        HregU16(40107, 8);
    }

    if(!allProducing) {
        HregU16(40071, 0); // Total Current AC = 0
        HregS16(40083,0);  // Total Power AC = 0
        return;
    }

    if(config.SunSpec.RemoteControl) {
        loopPowerLimit();
    }

    int16_t max_temp{0};
    uint16_t total_current_dc{0};
    int16_t total_power_dc{0};
    uint16_t max_voltage_dc{0};

    struct {
        uint16_t current{0};
        uint16_t voltage{0};
        uint16_t power{0};
        uint16_t power_factor{0};
        uint16_t count{0};
        int32_t energy{0};
        uint16_t frequency{0};
    } phases[3]{};

    int32_t total_energy{0};

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr) {
            continue;
        }

        auto stats = inv->Statistics();
        auto serial = inv->serial();
        auto conf = getConfig(serial);

        if(conf == nullptr || !conf->Enabled) {
            continue;
        }

        for (auto& t : stats->getChannelTypes()) {
            for (auto& c : stats->getChannelsByType(t)) {
                if (t == TYPE_AC) {
                    auto phase = conf->channel_ac[c].Phase;
                    phases[phase].current += (stats->getChannelFieldValue(t, c, FLD_IAC) * 100);
                    phases[phase].voltage = max(phases[phase].voltage, (uint16_t)(stats->getChannelFieldValue(t, c, FLD_UAC) * 10));
                    phases[phase].power += (uint16_t)(stats->getChannelFieldValue(t, c, FLD_PAC) * 10);
                    phases[phase].power_factor += (uint16_t)(stats->getChannelFieldValue(t, c, FLD_PF) * 100);
                    phases[phase].frequency += (uint16_t)(stats->getChannelFieldValue(t, c, FLD_F) * 10);
                    phases[phase].count++;
                }

                if (t == TYPE_DC) {
                    total_current_dc += (stats->getChannelFieldValue(t, c, FLD_IDC) * 100);
                    max_voltage_dc = max(max_voltage_dc, (uint16_t)(stats->getChannelFieldValue(t, c, FLD_UDC) * 10));
                    total_power_dc += (uint16_t)(stats->getChannelFieldValue(t, c, FLD_PDC) * 10);
                }

                if (t == TYPE_INV) {
                    total_energy += (uint32_t)(stats->getChannelFieldValue(t, c, FLD_YT) * 10 * 1000); // kWh -> Wh
                    max_temp = max(max_temp, (int16_t)(stats->getChannelFieldValue(t, c, FLD_T) * 10));
                }
            }
        }
    }

    auto total_current = phases[0].current + phases[1].current + phases[2].current;
    auto total_power = phases[0].power + phases[1].power + phases[2].power;
    auto count = (phases[0].count + phases[1].count+ phases[2].count);
    auto power_factor = (count > 0) ? (phases[0].power_factor + phases[1].power_factor + phases[2].power_factor) / count : 0;
    auto frequency = (count > 0) ? (phases[0].frequency + phases[1].frequency + phases[2].frequency) / count : 0;
    auto block = 100 + getPhaseCount();

    HregU16(40069, block);                  // Phase Configuration
    HregU16(40071, total_current);          // Total Current AC
    HregU16(40072, phases[0].current);      // Phase A Current
    HregU16(40073, phases[1].current);      // Phase B Current
    HregU16(40074, phases[2].current);      // Phase C Current
    HregU16(40076, phases[0].voltage);      // Phase Voltage AB
    HregU16(40087, phases[1].voltage);      // Phase Voltage BC
    HregU16(40088, phases[2].voltage);      // Phase Voltage CA
    HregU16(40079, phases[0].voltage);      // Phase Voltage AN
    HregU16(40080, phases[1].voltage);      // Phase Voltage BN
    HregU16(40081, phases[2].voltage);      // Phase Voltage CN
    HregS16(40083, total_power);            // Total Power AC
    HregU16(40086, frequency);              // Line Frequency
    HregU16(40091, power_factor);           // Power Factor
    HregS32(40093, total_energy);           // Total Energy kWh
    HregU16(40096, total_current_dc);       // Total Current DC
    HregU16(40100, total_power_dc);         // Total Power DC
    HregU16(40098, max_voltage_dc);         // Max Voltage DC
    HregU16(40102, max_temp);               // Temperature
    HregU16(40103, max_temp);               // Temperature
    HregU16(40104, max_temp);               // Temperature
    HregU16(40105, max_temp);               // Temperature
    HregU16(40124, getTotalMaxPower());     // WRtg
}

void ModbusSunSpecClass::setPowerLimit(uint16_t limit_pct, uint16_t timeout_sec) {
    const CONFIG_T& config = Configuration.get();

    MessageOutput.println("--------------");
    MessageOutput.println(" Power Limit");
    MessageOutput.println("--------------");

    uint16_t power_divider = config.SunSpec.PowerDivider;
    uint16_t total_max_power = getTotalMaxPower();
    uint16_t total_set_power = (total_max_power * limit_pct) / 100;

    // round total_set_power
    total_set_power = (total_set_power / power_divider) * power_divider;

    // new limit_pct
    limit_pct = (total_set_power * 100) / total_max_power;

    uint16_t total_power{0};
    int i{1};

    for (auto &conf : config.SunSpec.Inverter) {
        if (!conf.Enabled || conf.Serial == 0) {
            continue;
        }

        uint16_t curr_power = (conf.MaxPower * limit_pct) / 100;

        total_power += curr_power;

        auto limit = getLimit(conf.Serial);
        if (limit == nullptr) {
            MessageOutput.println("No empty inverter slot in max limit state!");
            continue;
        }

        if (limit->power != curr_power) {
            MessageOutput.printf("Inverter #%i: new power level: %iW (%u%%)\r\n", i++, curr_power, limit_pct);
            limit->pending = true;
            limit->power = curr_power;
        }

        limit->timeout = millis() + (timeout_sec > 0 ? timeout_sec * 1000 : 120 * 1000);
    }

    MessageOutput.printf("New Total Power: %uW\r\n", total_power);
}

uint16_t ModbusSunSpecClass::getTotalMaxPower() {
    uint16_t max_power{0};
    const CONFIG_T& config = Configuration.get();

    if(!config.SunSpec.RemoteControl) {
        return 0;
    }

    for (auto &conf : config.SunSpec.Inverter) {
        if (conf.Enabled && conf.Serial != 0) {
            max_power += conf.MaxPower;
        }
    }

    return max_power;
}

uint8_t ModbusSunSpecClass::getPhaseCount() {
    bool phases[3]{false};

    const CONFIG_T& config = Configuration.get();

    for (auto &conf : config.SunSpec.Inverter) {
        if(!conf.Enabled || conf.Serial == 0) {
            continue;
        }

        auto& phaselist = conf.channel_ac;
        for(int i = 0; i < 3; i++) {
            auto phase = phaselist[i].Phase;
            phases[phase] = true;
        }
    }

    uint8_t count{0};
    for(int i = 0; i < 3; i++) {
        count += phases[i] ? 1 : 0;
    }

    return count;
}

SUNSPEC_INVERTER_CONFIG_T* ModbusSunSpecClass::getConfig(uint64_t serial) {
    if(serial == 0) {
        return nullptr;
    }

    for(auto& conf: Configuration.get().SunSpec.Inverter) {
        if(conf.Serial == serial) {
            return &conf;
        }
    }

    return nullptr;
}

bool ModbusSunSpecClass::setLimit(uint64_t serial, uint16_t watts, const char *cause) {
    if(serial == 0) {
        return true;
    }

    auto inv = Hoymiles.getInverterBySerial(serial);
    if (!inv) {
        return true;
    }

    if(!inv->getRadio()->isQueueEmpty()) {
        return false;
    }

    if(inv->SystemConfigPara()->getLastLimitCommandSuccess() == CMD_PENDING) {
        return false;
    }

    MessageOutput.print("Inverter ");
    MessageOutput.print(serial, HEX);
    MessageOutput.printf(" sending power control request %uW (%s)\r\n", watts, cause);

    inv->sendActivePowerControlRequest(watts, PowerLimitControlType::AbsolutNonPersistent);
    return true;
}

ModbusSunSpecClass::Limit* ModbusSunSpecClass::getLimit(uint64_t serial) {
    for (auto &limit : _limit) {
        if (limit.serial == serial) {
            return &limit;
        }
    }

    for (auto &limit : _limit) {
        if (limit.serial == 0) {
            limit.serial = serial;
            limit.power = 0;
            limit.timeout = 0;
            return &limit;
        }
    }

    return nullptr;
}

void ModbusSunSpecClass::setManufacturerModel(const char* manufacturer, const char* model) {
    HregStr(40004, manufacturer, SUNSPEC_MAX_MANUFACTURER_STRLEN);
    HregStr(40020, model, SUNSPEC_MAX_MODEL_STRLEN);
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <TaskSchedulerDeclarations.h>
#include <ModbusTCP.h>

class ModbusSunSpecClass : protected ModbusTCP {
public:

    ModbusSunSpecClass();

    void init(Scheduler& scheduler);

    void setManufacturerModel(const char* manufacturer, const char* model);

protected:

    bool addHregS16(uint16_t offset, int16_t value);

    bool addHregU16(uint16_t offset, uint16_t value);

    bool addHregS32(uint16_t offset, int32_t value);

    bool addHregU32(uint16_t offset, uint32_t value);

    bool addHregStr(uint16_t offset, const String& value, uint16_t size);

    bool HregS16(uint16_t offset, int16_t value);

    bool HregU16(uint16_t offset, uint16_t value);

    bool HregS32(uint16_t offset, int32_t value);

    bool HregStr(uint16_t offset, const String& value, uint16_t size);

    uint16_t getTotalMaxPower();

    uint8_t getPhaseCount();

    SUNSPEC_INVERTER_CONFIG_T* getConfig(uint64_t serial);

private:

    struct Limit {
      uint64_t serial{0};
      int32_t timeout{0};
      int16_t power{0};
      bool pending{false};
    };

    void loop();

    void task();

    void loopPowerLimit();

    bool setLimit(uint64_t serial, uint16_t watts, const char *cause);

    Limit* getLimit(uint64_t serial);

    void setPowerLimit(uint16_t limit_pct, uint16_t timeout_sec);

    Limit _limit[INV_MAX_COUNT];

    Task _loopTask;

    Task _netTask;
};

extern ModbusSunSpecClass ModbusSunSpec;

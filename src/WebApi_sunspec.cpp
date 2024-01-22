// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2023 Thomas Basler and others
 */
#include "WebApi_sunspec.h"
#include "Configuration.h"
#include "Utils.h"
#include "WebApi.h"
#include "WebApi_errors.h"
#include "defaults.h"
#include "helper.h"
#include "ModbusSunSpec.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

void WebApiSunSpecClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    _server = &server;

    _server->on("/api/sunspec/config", HTTP_GET, std::bind(&WebApiSunSpecClass::onSunSpecGet, this, _1));
    _server->on("/api/sunspec/config", HTTP_POST, std::bind(&WebApiSunSpecClass::onSunSpecPost, this, _1));
}

void WebApiSunSpecClass::onSunSpecGet(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse(false, 100 * INV_MAX_COUNT);
    auto& root = response->getRoot();

    CONFIG_T& config = Configuration.get();
    root["enabled"] = config.SunSpec.Enabled;
    root["remote_control"] = config.SunSpec.RemoteControl;
    root["manufacturer"] = config.SunSpec.Manufacturer;
    root["model"] = config.SunSpec.Model;

    JsonArray data = root.createNestedArray("inverter");

    for (uint8_t i = 0; i < INV_MAX_COUNT; i++) {
        config.SunSpec.Inverter[i].Serial = config.Inverter[i].Serial;

        if (config.Inverter[i].Serial == 0) {
            continue;
        }

        JsonObject obj = data.createNestedObject();

        obj["id"] = i;
        obj["name"] = String(config.Inverter[i].Name);
        obj["order"] = config.Inverter[i].Order;
        obj["serial"] = config.SunSpec.Inverter[i].Serial;
        obj["enabled"] = config.SunSpec.Inverter[i].Enabled;

        auto inv = Hoymiles.getInverterBySerial(config.Inverter[i].Serial);

        uint8_t max_channels_ac{1};
        if (!inv) {
            obj["type"] = "Unknown";
            obj["max_power"] = config.SunSpec.Inverter[i].MaxPower;
        } else {
            obj["type"] = inv->typeName();
            max_channels_ac = inv->Statistics()->getChannelsByType(TYPE_AC).size();
            obj["max_power"] = (config.SunSpec.Inverter[i].MaxPower > 0) ? config.SunSpec.Inverter[i].MaxPower : inv->DevInfo()->getMaxPower();
        }

        JsonArray channel_ac = obj.createNestedArray("channel_ac");
        for (uint8_t c = 0; c < max_channels_ac; c++) {
            JsonObject chanData = channel_ac.createNestedObject();
            chanData["id"] = c;
            chanData["phase"] = config.SunSpec.Inverter[i].channel_ac[c].Phase;
        }
    }

    response->setLength();
    request->send(response);
}

void WebApiSunSpecClass::onSunSpecPost(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentials(request)) {
        return;
    }

    bool reboot{false};
    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& retMsg = response->getRoot();
    retMsg["type"] = "warning";

    if (!request->hasParam("data", true)) {
        retMsg["message"] = "No values found!";
        retMsg["code"] = WebApiError::GenericNoValueFound;
        response->setLength();
        request->send(response);
        return;
    }

    const String json = request->getParam("data", true)->value();

    if (json.length() > 1024) {
        retMsg["message"] = "Data too large!";
        retMsg["code"] = WebApiError::GenericDataTooLarge;
        response->setLength();
        request->send(response);
        return;
    }

    DynamicJsonDocument root(1024);
    const DeserializationError error = deserializeJson(root, json);

    if (error) {
        retMsg["message"] = "Failed to parse data!";
        retMsg["code"] = WebApiError::GenericParseError;
        response->setLength();
        request->send(response);
        return;
    }

    auto& config = Configuration.get();

    auto sunspec_enabled = root["enabled"].as<bool>();
    reboot |= (config.SunSpec.Enabled != sunspec_enabled);

    config.SunSpec.Enabled = sunspec_enabled;
    config.SunSpec.RemoteControl = root["remote_control"].as<bool>();
    strlcpy(config.SunSpec.Manufacturer, root["manufacturer"], sizeof(config.SunSpec.Manufacturer));
    strlcpy(config.SunSpec.Model, root["model"], sizeof(config.SunSpec.Model));

    ModbusSunSpec.setManufacturerModel(config.SunSpec.Manufacturer, config.SunSpec.Model);

    JsonArray inverterArray = root["inverter"].as<JsonArray>();
    if (inverterArray.size() > INV_MAX_COUNT) {
        retMsg["message"] = "Invalid amount of max channel setting given!";
        retMsg["code"] = WebApiError::InverterInvalidMaxChannel;
        response->setLength();
        request->send(response);
        return;
    }

    for (JsonVariant item : inverterArray) {
        if (!(item.containsKey("id") && item.containsKey("enabled") && item.containsKey("max_power") && item.containsKey("channel_ac"))) {
            retMsg["message"] = "Values are missing!";
            retMsg["code"] = WebApiError::GenericValueMissing;
            response->setLength();
            request->send(response);
            return;
        }

        if (item["id"].as<uint8_t>() > INV_MAX_COUNT - 1) {
            retMsg["message"] = "Invalid ID specified!";
            retMsg["code"] = WebApiError::InverterInvalidId;
            response->setLength();
            request->send(response);
            return;
        }

        JsonArray channelArrayAC = item["channel_ac"].as<JsonArray>();
        if (channelArrayAC.size() == 0 || channelArrayAC.size() > INV_MAX_CHAN_COUNT) {
            retMsg["message"] = "Invalid amount of max channel setting given!";
            retMsg["code"] = WebApiError::InverterInvalidMaxChannel;
            response->setLength();
            request->send(response);
            return;
        }

        SUNSPEC_INVERTER_CONFIG_T& inverter = config.SunSpec.Inverter[item["id"].as<uint8_t>()];

        inverter.Enabled = item["enabled"].as<bool>() | false;
        inverter.MaxPower = item["max_power"].as<uint16_t>() | 0;
        inverter.Serial = item["serial"].as<uint64_t>() | 0;

        for (JsonVariant channel : channelArrayAC) {
            auto id = channel["id"].as<uint8_t>();
            inverter.channel_ac[id].Phase = channel["phase"].as<uint8_t>();
        }
    }


    if(reboot) {
        WebApi.writeConfig(retMsg, WebApiError::MaintenanceRebootTriggered, "Inverter SunSpec configuration changed!");
        Utils::restartDtu();
    }
    else {
        WebApi.writeConfig(retMsg, WebApiError::SunSpecSettingsChanged, "Inverter SunSpec configuration changed!");
    }

    response->setLength();
    request->send(response);
}

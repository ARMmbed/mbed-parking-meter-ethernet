/**
 * @file    ParkingMeteConfiguration.h
 * @brief   mbed CoAP Endpoint Parking Meter Configuration Resource
 * @author  Doug Anson
 * @version 1.0
 * @see
 *
 * Copyright (c) 2014
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PARKING_METER_CONFIGURATION_RESOURCE_H__
#define __PARKING_METER_CONFIGURATION_RESOURCE_H__

// version info
#include "version.h"

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// JSON parser
#include "MbedJSONValue.h"

// Default configuration
#define DEFAULT_CONFIG "{\"free_parking\":0,\"no_beacon\":0}"

// instance reference
static void *__pkm_config = NULL;

/** ParkingMeterConfiguration class
 */
class ParkingMeterConfigurationResource : public DynamicResource
{

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
	ParkingMeterConfigurationResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"ParkingMeterConfiguration",M2MBase::GET_PUT_ALLOWED,observable) {
		__pkm_config = (void *)this;
		this.m_config = DEFAULT_CONFIG;
		this.update_config();
	}

    /**
    Get the value of the parking meter configuration
    @returns string containing the current JSON representation of our parking meter configuration
    */
    virtual string get() {
    	return this.m_config;
    }

    /**
    Set the value of the parking meter configuration
    @param string input for the parking meter configuration
    */
    virtual void put(const string value) {
    	this.m_config = value;
    }

    /**
     * FreeParking enabled/disabled
     */
    bool freeParkingEnabled() {
    	return this.m_free_parking;
    }

    /**
     * No Beacon mode enabled/disabled
     */
    bool noBeaconModeEnabled() {
    	return this.m_no_beacon;
    }

private:
    // parse the configuration JSON and update our configuration values
    void update_config() {
    	// parse the JSON
		MbedJSONValue parsed;
		parse(parsed,this.m_config.c_str());

		// pull the configuration values that are known...
		this.m_free_parking = (bool)parsed["free_parking"].get<int>();
		this.m_no_beacon = (bool)parsed["no_beacon"].get<int>();
    }

private:
    string m_config;

    bool m_free_parking;
    bool m_no_beacon;
};

// Free parking state
extern "C" bool freeParkingEnabled() {
	if (__pkm_config != NULL) {
		ParkingMeterConfigurationResource *pkm = (ParkingMeterConfigurationResource *)__pkm_config;
		return pkm->freeParkingEnabled();
	}
    return false;
}

// No beacon mode state
extern "C" bool noBeaconModeEnabled() {
	if (__pkm_config != NULL) {
		ParkingMeterConfigurationResource *pkm = (ParkingMeterConfigurationResource *)__pkm_config;
		return pkm->noBeaconModeEnabled();
	}
    return false;
}

#endif // __PARKING_METER_CONFIGURATION_RESOURCE_H__

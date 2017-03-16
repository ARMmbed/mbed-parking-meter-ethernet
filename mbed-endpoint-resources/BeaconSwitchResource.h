/**
 * @file    BeaconSwitchResource.h
 * @brief   mbed CoAP Endpoint Beacon Switch Resource
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

#ifndef __BEACON_SWITCH_RESOURCE_H__
#define __BEACON_SWITCH_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// our Digital out, tied to the reset line of the Cordio Beetle...
#if ENABLE_V2_RESOURCES
DigitalOut  __switch(D8);
#else
DigitalOut  __switch(D3);
#endif

// LED for confirmation
DigitalOut  __led(LED3);

// free parking state
static bool free_parking = false;

// Linkage to LCD Resource (for writing updates)
extern "C" void parking_meter_beacon_status(int status);

#if ENABLE_V2_RESOURCES
extern "C" void parking_status_led_yellow(bool on);
extern "C" void parking_status_led_red(bool on);
extern "C" void parking_status_led_green(bool on);
extern "C" void parking_status_led_blue(bool on);
#endif

// possible switch states
#define OFF             "0"     // Go HIGH... --> turn BLE OFF
#define ON              "1"     // Go LOW... --> turn BLE ON
#define NO_BEACON       "2"     // Go HIGH... --> turn BLE OFF

// external hooks for turning the beacon on and off
extern "C" void turn_beacon_on(void) {
#if ENABLE_V2_RESOURCES
	__switch = 1;
	parking_status_led_blue(true);
#else
    __switch = 0;
#endif
    __led = 0;
}
extern "C" void turn_beacon_off(void) {
#if ENABLE_V2_RESOURCES
	__switch = 0;
	parking_status_led_blue(false);
#else
    __switch = 1;
#endif
    __led = 1;
}

// Free parking state
extern "C" bool freeParkingEnabled() {
    return free_parking;
}

/** BeaconSwitchResource class
 */
class BeaconSwitchResource : public DynamicResource
{

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    BeaconSwitchResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"BeaconSwitch",M2MBase::GET_PUT_ALLOWED,observable) {
        // default is ON
        turn_beacon_on();
        parking_meter_beacon_status(true);
    }

    /**
    Get the value of the Beacon Switch
    @returns string containing either "0" (switch off) or "1" (switch on)
    */
    virtual string get() {
        string result(OFF);
        if (__switch) result = ON;
        return result;
    }

    /**
    Set the value of the Beacon Switch
    @param string input the string containing "0" (switch off) or "1" (switch on)
    */
    virtual void put(const string value) {
        if (value.compare(string(OFF)) == 0) {
            // FREE PARKING
            turn_beacon_off();
            free_parking = true;
            parking_meter_beacon_status(0);
        }
        else if (value.compare(string(NO_BEACON)) == 0) {
            // NO BEACON (PAID-FOR) PARKING
            turn_beacon_off();
            free_parking = false;
            parking_meter_beacon_status(2);
        }
        else {
            // PAID-FOR PARKING
            turn_beacon_on();
            free_parking = false;
            parking_meter_beacon_status(1);
        }
    }
};

#endif // __BEACON_SWITCH_RESOURCE_H__

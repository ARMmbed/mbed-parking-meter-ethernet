/**
 * @file    ParkingMeteConfiguration.h
 * @brief   mbed CoAP Endpoint Parking Meter Location Coords Resource
 * @author  Doug Anson
 * @version 1.0
 * @see
 *
 * Copyright (c) 2017
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

#ifndef __LOCATION_COORDS_RESOURCE_H__
#define __LOCATION_COORDS_RESOURCE_H__

// version info
#include "version.h"

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// JSON parser
#include "MbedJSONValue.h"

// Default configuration
// ARM Default: 30.243982, -97.844694
#define DEF_COORDS "{\"lat\":\"30.243982\",\"lng\":\"-97.844694\"}"

/** LocationCoordsResource class
 */
class LocationCoordsResource : public DynamicResource
{

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
	  LocationCoordsResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"LocCoordsResource",M2MBase::GET_PUT_ALLOWED,observable) {
		  this->m_coords = DEF_COORDS;
	  }

    /**
    Get the value of the parking meter configuration
    @returns string containing the current JSON representation of our parking meter configuration
    */
    virtual string get() {
    	return this->m_coords;
    }

    /**
    Set the value of the parking meter configuration
    @param string input for the parking meter configuration
    */
    virtual void put(const string value) {
    	this->m_coords = value;
    }

private:
    string m_coords;
};

#endif // __LOCATION_COORDS_RESOURCE_H__

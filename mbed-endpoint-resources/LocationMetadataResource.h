/**
 * @file    ParkingMeteConfiguration.h
 * @brief   mbed CoAP Endpoint Parking Meter Location Metadata Resource
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

#ifndef __LOCATION_METADATA_RESOURCE_H__
#define __LOCATION_METADATA_RESOURCE_H__

// version info
#include "version.h"

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// JSON parser
#include "MbedJSONValue.h"

// Time utils
#include "time_utils.h"

/*
 * metadata (1/2/2018)
	{
	  “id”: <unique number>,
	  “location”: {
	    “lat”: <floating point latitude of meter position>
	    “lng”: <floating point longitude of meter position
	  },
	  “installation_date”: “08/1/2017,08:06:20 UTC”,
	  “installer_id”: “<installer ID>”,
	  “installer_email”: “support@ParkingMetersSupply.com”,
	  “operational_state”: [“Available”,”Occupied”,”Maintenance”],
	  “operation_expiration”: “12/1/2017,23:15:00 UTC”
	  “hardware_version”: “1.0.0”,
	  ”firmware_version”: “2.3.4”,
	  “last_maintenance_date”: “12/1/2017,18:36:24 UTC”,
	  “last_maintenance_operation”: “FirmwareUpdate”,
	  “last_maintenance_operation_detail”: “1.2.3/2.3.4”,
	  “last_maintenance_status”: “Success”,
	  “current_time”: “12/1/2017,19:27:18 UTC”,
	  “metadata”: {
	    “spaceid”:<unique number>,
	    “regionid”:<unique number>,
			"lotid":<unique number>
	  }
	}
*	metadata (1/2/2018)
*/

// version info
#include "version.h"

// metadata defaults
#define DEF_ID 					 								0
#define DEF_INSTALLER_ID								0
#define DEF_INSTALL_DATE 								"1970-01-01,00:00:00,GMT"
#define DEF_INSTALLER_EMAIL 						"nobody@nowhere.com"
#define DEF_OPERATIONAL_STATE 					"Available"
#define DEF_OPERATION_EXPIRATION 				"1970-01-01,00:00:00,GMT"
#define DEF_HARDWARE_VERSION 						PKM_HW_VERSION
#define DEF_FIRMWARE_VERSION 						PKM_FW_VERSION
#define DEF_LAST_MAINTENANCE_DATE 			"1970-01-01,00:00:00,GMT"
#define DEF_LAST_MAINTENANCE_OPERATION 	"boot"
#define DEF_LAST_MAINTENANCE_DETAIL 		""
#define DEF_LAST_MAINTENANCE_STATUS 		"Success"
#define DEF_SPACE_ID 										0
#define DEF_REGION_ID 									0
#define DEF_LOT_ID 											0


/** LocationMetadataResource class
 */
class LocationMetadataResource : public DynamicResource
{

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
	LocationMetadataResource(const Logger *logger,const char *obj_name,const char *res_name,const LocationCoordsResource *coords, const bool observable = false) : DynamicResource(logger,obj_name,res_name,"LocMetadataResource",M2MBase::GET_PUT_ALLOWED,observable) {
		this->m_id = DEF_ID;
		this->m_installation_date = DEF_INSTALL_DATE;
		this->m_installer_id = DEF_INSTALLER_ID;
		this->m_installer_email = DEF_INSTALLER_EMAIL;
		this->m_operational_state = DEF_OPERATIONAL_STATE;
		this->m_operation_expiration = DEF_OPERATION_EXPIRATION;
		this->m_hardware_version = DEF_HARDWARE_VERSION;
		this->m_firmware_version = DEF_FIRMWARE_VERSION;
		this->m_last_maintenance_date = DEF_LAST_MAINTENANCE_DATE;
		this->m_last_maintenance_operation = DEF_LAST_MAINTENANCE_OPERATION;
		this->m_last_maintenance_detail = DEF_LAST_MAINTENANCE_DETAIL;
	  this->m_last_maintenance_status = DEF_LAST_MAINTENANCE_STATUS;
		this->m_space_id = DEF_SPACE_ID;
		this->m_region_id = DEF_REGION_ID;
		this->m_lot_id = DEF_LOT_ID;
		this->setLocationResource(coords);
	}

    /**
    Get the value of the parking meter configuration
    @returns string containing the current JSON representation of our parking meter configuration
    */
    virtual string get() {
    	return this->build_json();
    }

    /**
    Set the value of the parking meter configuration
    @param string input for the parking meter configuration
    */
    virtual void put(const string value) {
			this->update_from_json(value);
    }

		/**
		Set the peer location resource pointer
		*/
		void setLocationResource(const LocationCoordsResource *coords) {
			if (coords != NULL) {
				this->m_coords = (LocationCoordsResource *)coords;
			}
		}

private:
		// get the current time
		string current_time() {
			string time_str = get_current_time();
			return time_str;
		}

		// update from JSON instance
		void update_from_json(string json_str) {
			MbedJSONValue metadata_json;

			// parse the JSON
			parse(metadata_json,json_str.c_str());

			// update elements
			this->m_id = (unsigned int)metadata_json["id"].get<int>();
			this->m_installation_date = metadata_json["installation_date"].get<string>();
			this->m_installer_id = (unsigned int)metadata_json["installer_id"].get<int>();
			this->m_installer_email = metadata_json["installer_email"].get<string>();
			this->m_operational_state = metadata_json["operational_state"].get<string>();
			this->m_operation_expiration = metadata_json["operation_expiration"].get<string>();
			this->m_hardware_version = metadata_json["hardware_version"].get<string>();
			this->m_firmware_version = metadata_json["firmware_version"].get<string>();
			this->m_last_maintenance_date = metadata_json["last_maintenance_date"].get<string>();
			this->m_last_maintenance_operation = metadata_json["last_maintenance_operation"].get<string>();
			this->m_last_maintenance_detail = metadata_json["last_maintenance_operation_detail"].get<string>();
			this->m_last_maintenance_status = metadata_json["last_maintenance_status"].get<string>();

			// metadata details
			string metadata = metadata_json["metadata"].get<string>();
			MbedJSONValue metadata_values_json;
			parse(metadata_values_json,metadata.c_str());

			this->m_space_id = (unsigned int)metadata_values_json["spaceid"].get<int>();
			this->m_region_id = (unsigned int)metadata_values_json["regionid"].get<int>();
			this->m_lot_id = (unsigned int)metadata_values_json["lotid"].get<int>();
		}

	  // build into JSON format
		string build_json() {
		  MbedJSONValue metadata_json;
			MbedJSONValue coords_json;

			metadata_json["id"] = (int)this->m_id;
			if (this->m_coords != NULL) {
				string coords_str = this->m_coords->get();
				parse(coords_json,coords_str.c_str());
				metadata_json["location"] = coords_json;
			}
			metadata_json["installation_date"] = this->m_installation_date.c_str();
			metadata_json["installer_id"] = (int)this->m_installer_id;
			metadata_json["installer_email"] = this->m_installer_email.c_str();
			metadata_json["operational_state"] = this->m_operational_state.c_str();
			metadata_json["operation_expiration"] = this->m_operation_expiration.c_str();
			metadata_json["hardware_version"] = this->m_hardware_version.c_str();
			metadata_json["firmware_version"] = this->m_firmware_version.c_str();
			metadata_json["last_maintenance_date"] = this->m_last_maintenance_date.c_str();
			metadata_json["last_maintenance_operation"] = this->m_last_maintenance_operation.c_str();
			metadata_json["last_maintenance_operation_detail"] = this->m_last_maintenance_detail.c_str();
			metadata_json["last_maintenance_status"] = this->m_last_maintenance_status.c_str();
			metadata_json["current_time"] = this->current_time().c_str();

			MbedJSONValue metadata_values_json;
			metadata_values_json["spaceid"] = (int)this->m_space_id;
			metadata_values_json["regionid"] = (int)this->m_region_id;
			metadata_values_json["lotid"] = (int)this->m_lot_id;

			metadata_json["metadata"] = metadata_values_json;

			return metadata_json.serialize();
		}

		// private members
		LocationCoordsResource *m_coords;
		unsigned int 		        m_id;
		string 							    m_installation_date;
		unsigned int 				    m_installer_id;
		string 							    m_installer_email;
		string                  m_operational_state;
		string 							    m_operation_expiration;
		string 							    m_hardware_version;
		string 							    m_firmware_version;
		string							    m_last_maintenance_date;
		string                  m_last_maintenance_operation;
		string 							    m_last_maintenance_detail;
	  string							    m_last_maintenance_status;
		unsigned int            m_space_id;
		unsigned int            m_region_id;
		unsigned int            m_lot_id;
};

#endif // __LOCATION_METADATA_RESOURCE_H__

/**
 * @file    main.cpp
 * @brief   mbed Endpoint Sample main
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

// versioning info
#include "version.h"

// V2 Resources
#if ENABLE_V2_RESOURCES
	// CUSTOMIZE ME: Define the core Device Types, Firmware, Hardware, Software information
	#define ENABLE_DEVICE_MANAGER	false				// true - enable, false - disable
	#define MY_DEVICE_MFG			"ARM/NXP"
#if ENABLE_V2_COMPAT
	#define MY_DEVICE_TYPE			"parking-meter"
#else
	#define MY_DEVICE_TYPE			"parking-meter-v2"
#endif
	#define MY_DEVICE_MODEL			"K64F"
	#define MY_DEVICE_SERIAL 		"0123456789"
	#define MY_FIRMWARE_VERSION		"2.0.0"
	#define MY_HARDWARE_VERSION		"2.0.0"
	#define MY_SOFTWARE_VERSION		"2.0.0"

#else
	// CUSTOMIZE ME: Define the core Device Types, Firmware, Hardware, Software information
	#define ENABLE_DEVICE_MANAGER	false				// true - enable, false - disable
	#define MY_DEVICE_MFG			"NXP"
	#define MY_DEVICE_TYPE			"parking-meter"
	#define MY_DEVICE_MODEL			"K64F"
	#define MY_DEVICE_SERIAL 		"0123456789"
	#define MY_FIRMWARE_VERSION		"1.0.0"
	#define MY_HARDWARE_VERSION		"1.0.0"
	#define MY_SOFTWARE_VERSION		"1.0.0"
#endif

// Passphrase to supply for data management authentication
#define MY_DM_PASSPHRASE		"arm1234"

// Override: enable use of PUT to start parking countdown (default true)
#define ENABLE_PUT_TO_START		true

// Include security.h
#include "security.h"

// mbed Endpoint Network
#include "mbed-connector-interface/mbedEndpointNetwork.h"

// Logger
#include "mbed-connector-interface/Logger.h"
Serial pc(USBTX,USBRX);
Logger logger(&pc);

// ConnectionHandler support
#define ENABLE_CONNECTION_HANDLER	true			// true - enable, false - disable
#include "ConnectionHandler.h"

// Include the default Device Management Responders
#include "dm-responders/ResponderFunctions.h"

// Our Device Management Authenticator (trivial passphrase authenticator used)
#include "mbed-connector-interface/PassphraseAuthenticator.h"
PassphraseAuthenticator authenticator(&logger,MY_DM_PASSPHRASE);

// Our Device Management Responder
#include "mbed-connector-interface/DeviceManagementResponder.h"

// Our Device Manager
#include "mbed-connector-interface/DeviceManager.h"

// LCD Resource
#include "mbed-endpoint-resources/LCDResource.h"
LCDResource lcd(&logger,"312","1");						

// HourGlass Resource
#include "mbed-endpoint-resources/HourGlassResource.h"
HourGlassResource hourglass(&logger,"100","1",true);

// BLE Beacon Switch Resource
#include "mbed-endpoint-resources/BeaconSwitchResource.h"
BeaconSwitchResource beacon_switch(&logger,"200","1");

// configuration resource
#include "mbed-endpoint-resources/ParkingMeterConfigurationResource.h"
ParkingMeterConfigurationResource config_resource(&logger,"201","1");

// Initialize the LED and LCD
extern "C" void init_lcd_and_leds();

// V2 Resources
#if ENABLE_V2_CAMERA
	// V2: Camera
	#include "mbed-endpoint-resources/CameraResource.h"
	CameraResource camera(&logger,"300","1",true,&authenticator);
#endif

#if ENABLE_V2_OCCUPANCY_DETECTOR
	// V2: ParkingStallOccupancyDetectorResource
	#include "mbed-endpoint-resources/ParkingStallOccupancyDetectorResource.h"
	ParkingStallOccupancyDetectorResource occupancy_detector(&logger,"400","1",true);
#endif // ENABLE_V2_RESOURCES

// called from the Endpoint::start() below to create resources and the endpoint internals...
Connector::Options *configure_endpoint(Connector::OptionsBuilder &config)
{    
    // Build the endpoint configuration parameters
    logger.log("Endpoint::main (%s): customizing endpoint configuration...",net_get_type());
    return config                 
    	// PROVISIONING: set the Provisioning Credentials (all from security.h)
        .setEndpointNodename(MBED_ENDPOINT_NAME)                  			
        .setDomain(MBED_DOMAIN)
        .setEndpointType(MY_DEVICE_TYPE)                               							  
        .setServerCertificate((uint8_t *)SERVER_CERT,(int)sizeof(SERVER_CERT))
        .setClientCertificate((uint8_t *)CERT,(int)sizeof(CERT))
        .setClientKey((uint8_t *)KEY,(int)sizeof(KEY))

        // set CoAP use of TCP or UDP (default is UDP)
        //.setCoAPConnectionType(COAP_UDP)
        .setCoAPConnectionType(COAP_TCP)
                   
        // Add my specific physical dynamic resources...
        .addResource(&lcd)
	//.addResource(&config_resource)
        .addResource(&hourglass,(bool)false) 			// on-demand observations...
        .addResource(&beacon_switch)		

// V2 Resources        
#if ENABLE_V2_CAMERA
	.addResource(&camera,(bool)false)		// observation issued after POST operation completes...
#endif
#if ENABLE_V2_OCCUPANCY_DETECTOR
	.addResource(&occupancy_detector,(bool)false)	// observation issued upon motion detection...
#endif			
                   
        // finalize the configuration...
        .build();
}

// reference to the time initializer...
extern "C" void init_time(void);

// main entry point...
int main()
{
    // set Serial
    pc.baud(115200);
	
    // Announce
    logger.log("\r\n\r\nmbed Parking Meter (%s)",net_get_type());

    // LCD Update
    write_parking_meter_title((char *)MY_FIRMWARE_VERSION);

    // initialize LCD and LEDs
    init_lcd_and_leds();
    
    // Configure Device Manager (if enabled)
    DeviceManager *device_manager = NULL;
    if (ENABLE_DEVICE_MANAGER) {
	    // Allocate the Device Management Components
	    DeviceManagementResponder *dm_processor = new DeviceManagementResponder(&logger,&authenticator);
	    device_manager = new DeviceManager(&logger,dm_processor,MY_DEVICE_MFG,MY_DEVICE_TYPE,MY_DEVICE_MODEL,MY_DEVICE_SERIAL,MY_FIRMWARE_VERSION,MY_HARDWARE_VERSION,MY_SOFTWARE_VERSION);
		
	    // Register the default Device Management Responders
	    dm_processor->setInitializeHandler(dm_initialize);
	    dm_processor->setRebootResponderHandler(dm_reboot_responder);
	    dm_processor->setResetResponderHandler(dm_reset_responder);
	    dm_processor->setFOTAManifestHandler(dm_set_manifest);
	    dm_processor->setFOTAImageHandler(dm_set_fota_image);
	    dm_processor->setFOTAInvocationHandler(dm_invoke_fota);
    }
     
    // we have to plumb our network first
    Connector::Endpoint::plumbNetwork((void *)device_manager);
    
    // initialize time for the endpoint
    init_time();

    // Set our ConnectionHandler instance (after plumbing the network...)
    if (ENABLE_CONNECTION_HANDLER) {
    	Connector::Endpoint::setConnectionStatusInterface(new ConnectionHandler());
    }
             
    // starts the endpoint by finalizing its configuration (configure_endpoint() above called),creating a Thread and reading mbed Cloud events...
    Connector::Endpoint::start();
}

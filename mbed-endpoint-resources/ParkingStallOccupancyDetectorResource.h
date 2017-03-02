/**
 * @file    ParkingStallOccupancyDetectorResource.h
 * @brief   mbed CoAP Endpoint Parking Stall Occupancy Detector Resource
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

#ifndef __RANGE_FINDER_RESOURCE_H__
#define __RANGE_FINDER_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// RangeFinder 
#include "RangeFinder.h"

// JSON parsing support
#include "MbedJSONValue.h"

// Seeed ultrasound range finder
static RangeFinder __range_finder(D2, 10, 5800.0, 100000);

// Our wait time between checks for range detection (in ms)
#define WAIT_TIME                   250    // .25 seconds between range checks

// Status String length
#define STATUS_STRING_LENGTH		64

// Values of our resource
#define EMPTY_STR          			"0"     // empty
#define OCCUPIED_STR				"1"		// object present
#define ARRIVING_STR             	"2"     // object arriving
#define DEPARTING_STR				"3"		// object leaving

// default arrival rate (minimum rate that notes change transition occuring)
#define DEFAULT_MOVEMENT_RATE_M_S	0.02	// 0.02 m/sec

// default occupied threshold (M)
#define DEFAULT_OCCUPIED_RANGE_M	0.2		// object with 0.1m of camera... stationary

// default arriving threshold (M)
#define DEFAULT_ARRIVING_RANGE_M	0.3		// object within 0.2m of camera.. arriving (rate positive)

// default departing threshold
#define DEFAULT_DEPARTING_RANGE_M 	0.4		// object within 0.3m of camera... departing (rate negative)

// default MAX range
#define DEFAULT_MAX_RANGE_M			0.5		// longest range (in m) we care about... beyond which we dont care...

// forward declarations
static void *_instance = NULL;

// observation latch
static bool __observation_latch = true;

// reset the observation latch
extern "C" void reset_observation_latch() {
    __observation_latch = true;
}

// parking stall state updater forward reference
extern "C" void _update_parking_stall_state(const void *args);

// parking stall states
enum ParkingStallStates {
	STALL_EMPTY=0,			// stall is EMPTY
	STALL_OCCUPIED=1,			// stall is OCCUPIED
	STALL_ARRIVING=2,			// stall has car arriving into it
	STALL_DEPARTING=3,		// stall has car departing from it
	STALL_NUM_STATES=4		// number of states
};

/** ParkingStallOccupancyDetectorResource class
 */
class ParkingStallOccupancyDetectorResource : public DynamicResource
{
private:
    Thread         	   *m_parking_stall_state_transitioner;
    int					m_wait_time;
    float           	m_range;
    float			    m_last_range;
    float				m_rate_m_s;
    float				m_min_rate;
    float           	m_occupied_range;
    float		    	m_arriving_range;
    float		    	m_departing_range;
    float				m_max_range;
    string          	m_parking_stall_state_str;
    bool            	m_perform_observation;
    int					m_counter;
    ParkingStallStates  m_state;

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    ParkingStallOccupancyDetectorResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"OccupancyDetector",M2MBase::GET_PUT_ALLOWED,observable) {
        // init
        _instance = (void *)this;
        
        // initialize default states
        this->m_range = -1.0;
        this->m_last_range = -1.0;
        this->m_wait_time = WAIT_TIME;
        this->m_counter = 0;
        this->m_parking_stall_state_str = EMPTY_STR;
        this->m_state = STALL_EMPTY;

        // default configuration
        this->m_min_rate = DEFAULT_MOVEMENT_RATE_M_S;
        this->m_occupied_range = DEFAULT_OCCUPIED_RANGE_M;
        this->m_arriving_range = DEFAULT_ARRIVING_RANGE_M;
        this->m_departing_range = DEFAULT_DEPARTING_RANGE_M;
        this->m_max_range = DEFAULT_MAX_RANGE_M;
        
        // no Thread yet
        this->m_parking_stall_state_transitioner = NULL;
        
        // no observation yet
        this->m_perform_observation = false;
    }

    /**
    Get the ParkingStallOccupancyDetectorResource value
    @returns string containing the current state of the parking stall
    */
    virtual string get() {
        // we have to wait until the main loop starts in the endpoint before we create our thread... 
        if (this->m_parking_stall_state_transitioner == NULL) {
            // create the processing thread
            this->m_parking_stall_state_transitioner = new Thread(_update_parking_stall_state,NULL,osPriorityNormal,DEFAULT_STACK_SIZE);
        }
        
        // return our latest range status
        return this->m_parking_stall_state_str;
    }
    
    /**
    Set the configuration for the parking stall occupancy detector
    JSON format: {"min_rate":0.02,"occupied_range":0.2,"arrival_range": 0.3,"departing_range":0.4,"max_range":0.5}
    min_rate - the minimum rate to indicate "movemment" and is directional (negative: toward camera, positive: away from camera)
    occupied_range - range from the camera when a car is parked in the stall
    arrival_range - >= this range indicates a car arriving into the stall (closure rate is positive)
    departure_range - <= this range indicates a car departing the stall (closure rate is negative)
    max_range - maximum range beyond which we dont care what happens
    @param string input the string the maximum trigger range in meters
    */
    virtual void put(const string value) {
    	// JSON parser
    	MbedJSONValue parsed;

    	// parse the PUT value
    	parse(parsed, value.c_str());

    	// pull the management values out
    	this->m_min_rate = (float)parsed["min_rate"].get<double>();
    	this->m_occupied_range = (float)parsed["occupied_range"].get<double>();
    	this->m_arriving_range = (float)parsed["arrival_range"].get<double>();
    	this->m_departing_range = (float)parsed["departing_range"].get<double>();
    	this->m_max_range = (float)parsed["max_range"].get<double>();
        
        // DEBUG
        this->logger()->log("OccupancyDetector: min_rate: %.1f occupied: %.1f arriving: %.1f departing: %.1f max_range: %.1f",
        		this->m_min_rate,this->m_occupied_range,this->m_arriving_range,this->m_departing_range,this->m_max_range);
    }
    
    // get the wait time
    int get_wait_time() {
    	return this->m_wait_time;
    }

    // call to perform an observation if needed
    void update_parking_stall_state() {
        // get the latest range values
        this->get_range();
        
        // update our status
        this->parking_stall_state_transitioner();
        
        // set our value and create an observation event
        if (this->m_perform_observation == true && __observation_latch == true) {
        	// DEBUG
        	this->logger()->log("ParkingStallOccupancyDetectorResource: Sending observation....");
            this->observe();

            // reset latch
            this->logger()->log("ParkingStallOccupancyDetectorResource: Latching until we are done with the camera...");
            __observation_latch = false;
        }
        else if (this->m_perform_observation == true) {
        	// latch locked... not observing...
        	//this->logger()->log("ParkingStallOccupancyDetectorResource: NOT observing... latch still locked...");
        }
        else {
        	// DEBUG nothing to observe
        	//this->logger()->log("ParkingStallOccupancyDetectorResource: Nothing to observe (OK)");
        }
    }
    
private:
    // get the latest range value
    void get_range() {
    	this->m_last_range = this->m_range;
    	this->m_range = __range_finder.read_m();
    	this->m_rate_m_s = (this->m_range - this->m_last_range)/(this->get_wait_time()/1000.0);
        
        // DEBUG
        //this->logger()->log("ParkingStallOccupancyDetectorResource: Range: %.1f",this->m_range);
    }
    
    // create our status string
    string create_status_string(ParkingStallStates status) {
    	char buf[STATUS_STRING_LENGTH+1];
		memset(buf,0,STATUS_STRING_LENGTH+1);
		++this->m_counter;
		sprintf(buf,"{\"count\":%d,\"state\":%d}",this->m_counter,(int)status);
		return string(buf);
    }

    // update our parking stall state
    void parking_stall_state_transitioner() {
        // reset our observation state
        this->m_perform_observation = false;
        
        // if thresholds crossed... record and perform an observation...
        if (this->m_range == -1.0)  {
            // ERROR in the sensor
            this->logger()->log("ParkingStallOccupancyDetectorResource: Timeout Error in range finder sensor");
            this->m_parking_stall_state_str = this->create_status_string(STALL_EMPTY);
            this->m_state = STALL_EMPTY;
            this->m_rate_m_s = 0.0;
        } 
        else if (this->m_range >= this->m_max_range) {
			// stall is EMPTY
			//this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall is EMPTY");
			this->m_parking_stall_state_str = this->create_status_string(STALL_EMPTY);
			this->m_state = STALL_EMPTY;
			this->m_rate_m_s = 0.0;
		}
        else if (this->m_range > this->m_departing_range && this->m_rate_m_s == 0.0 && (this->m_state == STALL_OCCUPIED || this->m_state == STALL_DEPARTING || this->m_state == STALL_ARRIVING)) {
            // stall has just turned EMPTY
            //this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall has just turned EMPTY");
            this->m_parking_stall_state_str = this->create_status_string(STALL_EMPTY);
            this->m_rate_m_s = 0.0;

            if (this->m_state != STALL_EMPTY) {
				// generate an observation
				this->m_perform_observation = true;
				this->m_parking_stall_state_str = this->create_status_string(STALL_EMPTY);
            }
            this->m_state = STALL_EMPTY;
        } 
        else {
        	// DEBUG
            this->logger()->log("RangeDetector: Distance: %.1f m, rate: %.2f m/s",this->m_range,this->m_rate_m_s);

            //
            // object detected in range.. we must determine the stall state
            //
            // (EMPTY | ARRIVING) -> OCCUPIED
            if (this->m_range <= this->m_occupied_range && this->m_rate_m_s == 0 && (this->m_state == STALL_EMPTY || this->m_state == STALL_ARRIVING)) {
            	// DEBUG
            	this->logger()->log("ParkingStallOccupancyDetectorResource: stall is now OCCUPIED...");

            	// generate an observation
            	this->m_perform_observation = true;
            	this->m_parking_stall_state_str = this->create_status_string(STALL_OCCUPIED);

            	// XXX set any LED statuses - we are now OCCUPIED
            }

            // (ARRIVING | EMPTY) -> ARRIVING
            else if (this->m_range <= this->m_arriving_range && this->m_range > this->m_occupied_range && this->m_rate_m_s <= -(this->m_min_rate) && (this->m_state == STALL_ARRIVING || this->m_state == STALL_EMPTY)) {
				// DEBUG
				this->logger()->log("ParkingStallOccupancyDetectorResource: stall has ARRIVING car...");

				// XXX set any LED statuses - car is ARRIVING
			}

            // (OCCUPIED | DEPARTING) -> DEPARTING
            else if (this->m_range >= this->m_departing_range && this->m_rate_m_s >= this->m_min_rate && (this->m_state == STALL_EMPTY || this->m_state == STALL_OCCUPIED || this->m_state == STALL_DEPARTING || this->m_state == STALL_ARRIVING)) {
				// DEBUG
				this->logger()->log("ParkingStallOccupancyDetectorResource: stall has DEPARTING car...");

				// XXX set any LED statuses - car is DEPARTING
			}
        }
    }
};

// update our parking stall state (THREAD)
void _update_parking_stall_state(const void *args) {
	int wait_time = WAIT_TIME;

	// wait a bit initially
	Thread::wait(4*wait_time);

	// loop forever...
    while (true) {
        if (_instance != NULL) {
            // update our parking stall state
            ((ParkingStallOccupancyDetectorResource *)_instance)->update_parking_stall_state();

            // update the wait time
            wait_time = ((ParkingStallOccupancyDetectorResource *)_instance)->get_wait_time();
        }
        
        // wait a bit and look again...
        Thread::wait(wait_time);
    }
}

#endif // __RANGE_FINDER_RESOURCE_H__

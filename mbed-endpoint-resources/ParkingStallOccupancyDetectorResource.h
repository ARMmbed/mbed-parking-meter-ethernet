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

#ifndef __PARKING_STALL_OCCUPANCY_DETECTOR_RESOURCE_H__
#define __PARKING_STALL_OCCUPANCY_DETECTOR_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// RangeFinder 
#include "RangeFinder.h"

// JSON parsing support
#include "MbedJSONValue.h"

// hook for turning the beacon on/off
extern "C" void turn_beacon_on(void);
extern "C" void turn_beacon_off(void);

// Seeed ultrasound range finder
static RangeFinder __range_finder(D2, 10, 5800.0, 100000);

// Our wait time between checks for range detection (in ms)
#define WAIT_TIME                   1000    // 1 seconds between range checks

// High resolution wait time
#define HREZ_WAIT_TIME				150		// 150ms between range checks

// Status String length
#define STATUS_STRING_LENGTH		64

// Values of our resource
#define EMPTY_STR          			"0"     // empty
#define OCCUPIED_STR				"1"		// object present
#define ARRIVING_STR             	"2"     // object arriving
#define DEPARTING_STR				"3"		// object leaving

// CONFIG: {"min_move_rate":0.03,"occupied_range":0.12,"max_range":0.37,"occupied_variance":0.01,"range_end":0.60}

// minimum movement rate (denotes movement vs. non-movement)
#define DEFAULT_MOVEMENT_RATE_M_S	0.03	// +-0.03 m/sec

// default occupied threshold (M)
#define DEFAULT_OCCUPIED_RANGE_M	0.12	// object with 0.12m of camera... stationary

// default maximum variance for the occupied threshold (M)
#define DEFAULT_OCCUPIED_VARIANCE_M 0.01	// +- this is considered "occupied"

// default MAX range
#define DEFAULT_MAX_RANGE_M			0.37	// longest range (in m) we care about... beyond which we dont care...

// range END
#define DEFAULT_RANGE_END_M			0.60	// < MAX_RANGE and beyond which, we set the range value to "OUT_OF_RANGE"

// default OUT OF RANGE value
#define DEFAULT_OUT_OF_RANGE	    100.0	// defaulted out of range range value...

// forward declarations
static void *_instance = NULL;

// observation latch: true - observation allowed, false - observation not allowed
static bool __observation_latch = true;

// reset the observation latch forward reference
extern "C" void reset_observation_latch();

// parking stall state updater forward reference
extern "C" void _update_parking_stall_state(const void *args);

// LED togglers
extern "C" void parking_status_led_red(bool on);
extern "C" void parking_status_led_yellow(bool on);
extern "C" void parking_status_led_green(bool on);
extern "C" void parking_status_led_blue(bool on);

// parking stall states
enum ParkingStallStates {
	STALL_EMPTY=0,			// stall is EMPTY
	STALL_OCCUPIED=1,		// stall is OCCUPIED
	STALL_ARRIVING=2,		// stall has car arriving into it
	STALL_DEPARTING=3,		// stall has car departing from it
	STALL_NUM_STATES=4		// number of states
};

// parking stall movement direction
enum MovementDirection {
	INTO_STALL=0,			// movement into the stall
	NO_MOVEMENT=1,			// no movement
	OUT_OF_STALL=2			// movement out of the stall
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
    float				m_min_rate;
    float           	m_occupied_range;
    float			    m_max_occupied_range_variance;
    float				m_max_range;
    float 				m_range_end;
    string          	m_parking_stall_state_str;
    bool            	m_perform_observation;
    bool				m_state_change;
    MovementDirection	m_movement;
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
        this->m_range = DEFAULT_OUT_OF_RANGE;
        this->m_last_range = DEFAULT_OUT_OF_RANGE;
        this->m_wait_time = WAIT_TIME;
        this->m_counter = 0;
        this->m_movement = NO_MOVEMENT;
        this->m_parking_stall_state_str = EMPTY_STR;
        this->m_state = STALL_EMPTY;

        // default configuration
        this->m_min_rate = DEFAULT_MOVEMENT_RATE_M_S;
        this->m_occupied_range = DEFAULT_OCCUPIED_RANGE_M;
        this->m_max_range = DEFAULT_MAX_RANGE_M;
        this->m_range_end = DEFAULT_RANGE_END_M;
        this->m_max_occupied_range_variance = DEFAULT_OCCUPIED_VARIANCE_M;
        
        // no Thread yet
        this->m_parking_stall_state_transitioner = NULL;
        
        // no observation yet
        this->m_perform_observation = false;
        this->m_state_change = false;
    }

    /**
    Get the ParkingStallOccupancyDetectorResource value
    @returns string containing the current state of the parking stall
    */
    virtual string get() {
        // we have to wait until the main loop starts in the endpoint before we create our thread... 
        if (this->m_parking_stall_state_transitioner == NULL) {
            // create the processing thread
            this->m_parking_stall_state_transitioner = new Thread();
            if (this->m_parking_stall_state_transitioner != NULL) {
            	this->m_parking_stall_state_transitioner->start(callback(_update_parking_stall_state,(const void *)NULL));
            }
            else {
            	this->logger()->log("ParkingStallOccupancyDetectorResource: unable to allocate Thread. Aborting...");
            }
        }
        
        // return our latest range status
        return this->m_parking_stall_state_str;
    }
    
    /**
    Set the configuration for the parking stall occupancy detector
    JSON format: {"min_move_rate":0.03,"occupied_range":0.12,"max_range":0.37,"occupied_variance":0.01,"range_end":0.50}
    min_move_rate - the minimum rate to indicate "movemment" and is directional (negative: toward camera, positive: away from camera)
    occupied_range - range from the camera when a car is parked in the stall
    max_range - maximum range beyond which we dont care what happens
    occupied_variance - amount of "variance" we can have to accept the range as "occupied"
    @param string input the string containing a JSON in the above format
    */
    virtual void put(const string json) {
    	// JSON parser
    	MbedJSONValue parsed;

    	// parse the PUT JSON
    	parse(parsed, json.c_str());

    	// set the configuration
    	this->m_min_rate = (float)parsed["min_move_rate"].get<double>();
    	this->m_occupied_range = (float)parsed["occupied_range"].get<double>();
    	this->m_max_range = (float)parsed["max_range"].get<double>();
    	this->m_max_occupied_range_variance = (float)parsed["occupied_variance"].get<double>();
    	this->m_range_end = (float)parsed["range_end"].get<double>();
        
        // DEBUG
        this->logger()->log("ParkingStallOccupancyDetectorResource: min_rate: %.1f occupied: %.1f max_range: %.1f occupied_variance: %.2f",
        		this->m_min_rate,this->m_occupied_range,this->m_max_range,this->m_max_occupied_range_variance);
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
        if (this->m_perform_observation == true && __observation_latch == true && this->m_state_change == true) {
        	// DEBUG
        	this->logger()->log("ParkingStallOccupancyDetectorResource: Sending observation....");
            this->observe();

            // reset latch
            this->logger()->log("ParkingStallOccupancyDetectorResource: Latching until we are done with the camera...");
            __observation_latch = false;

            // observation sent for this state once (state must change prior to another being sent)
            this->m_state_change = false;
        }
        else if (this->m_perform_observation == true && this->m_state_change == true) {
        	// latch locked... not observing...
        	this->logger()->log("ParkingStallOccupancyDetectorResource: NOT observing... latch still locked...");
        }
        else if (this->m_perform_observation == true && __observation_latch == true) {
			// already observed for this particular state change event
			this->logger()->log("ParkingStallOccupancyDetectorResource: already observed for this state change (OK)...");
		}
        else {
        	// DEBUG nothing to observe
        	//this->logger()->log("ParkingStallOccupancyDetectorResource: Nothing to observe (OK)");
        	reset_observation_latch();
        	this->m_state_change = false;
        }
    }
    
private:
    // LED annunciations
    void led_stall_empty() {
    	parking_status_led_red(false);
    	parking_status_led_yellow(false);
    	parking_status_led_green(false);
    	parking_status_led_blue(true);
    }
    void led_stall_arriving() {
		parking_status_led_red(false);
		parking_status_led_yellow(true);
		parking_status_led_green(false);
		parking_status_led_blue(false);
    }
    void led_stall_occupied() {
		parking_status_led_red(true);
		parking_status_led_yellow(false);
		parking_status_led_green(false);
		parking_status_led_blue(false);
	}
    void led_stall_departing() {
		parking_status_led_red(false);
		parking_status_led_yellow(true);
		parking_status_led_green(false);
		parking_status_led_blue(false);
	}

    // get the latest range value
    void get_range() {
    	float new_range = __range_finder.read_m();
    	this->m_movement = NO_MOVEMENT;
    	if (new_range < 0) {
    		// ERROR: set everything to 0
			this->m_last_range = DEFAULT_OUT_OF_RANGE;
			this->m_range = DEFAULT_OUT_OF_RANGE;
    	}
    	else {
			this->m_last_range = this->m_range;
			this->m_range = new_range;
			float rate_m_s = (this->m_range - this->m_last_range)/(this->get_wait_time()/1000.0);

			if (this->m_last_range >= DEFAULT_OUT_OF_RANGE || this->m_range >= DEFAULT_OUT_OF_RANGE) {
				// zero out
				rate_m_s = 0.0;
			}

			float fabs_rate = fabs(rate_m_s);
			if (fabs_rate > 0.0 && fabs_rate < this->m_min_rate) {
				this->m_movement = NO_MOVEMENT;
			}
			if (rate_m_s >= DEFAULT_MOVEMENT_RATE_M_S) {
				this->m_movement = OUT_OF_STALL;
			}
			else if (rate_m_s <= -(DEFAULT_MOVEMENT_RATE_M_S)) {
				this->m_movement = INTO_STALL;
			}

			//
			// now that we have movement... cap the range.. we can watch it move beyond max_range...
			// but beyond range_end we set to out_of_range...
			//
			if (new_range > this->m_range_end) {
				// set everything to 0
				this->m_last_range = DEFAULT_OUT_OF_RANGE;
				this->m_range = DEFAULT_OUT_OF_RANGE;
			}
    	}
        
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

    // setup for an observation event
    void enable_observation() {
    	this->logger()->log("ParkingStallOccupancyDetectorResource: ENABLE observation...");
    	this->m_perform_observation = true;
    }

    // disable observation
    void disable_observation() {
    	this->logger()->log("ParkingStallOccupancyDetectorResource: DISABLE observation...");
    	this->m_perform_observation = false;
    }

    // we are within our parking range
    bool within_parking_range(float range) {
    	bool is_parked = false;

    	// create our range
    	float plus_range = this->m_occupied_range + this->m_max_occupied_range_variance;
    	float minus_range = 0.0; // make sure we dont bump the meter! this->m_occupied_range - this->m_max_occupied_range_variance;

    	// see if our input range lies inbetwee the values
    	if (range >= minus_range && range <= plus_range) {
    		is_parked = true;
    	}

    	// return our parking range status
    	return is_parked;
    }

    // update our parking stall state
    void parking_stall_state_transitioner() {
		// DEBUG
		//this->logger()->log("ParkingStallOccupancyDetectorResource: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);

        // reset our observation state
        this->m_perform_observation = false;
        
        // handle cases of suddenly out of range... just freeze at the current last known state...
        if (this->m_range >= DEFAULT_OUT_OF_RANGE && this->m_state == STALL_EMPTY) {
			// stall is still EMPTY
			//this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall is EMPTY");
			//this->logger()->log("EMPTY: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);
		}
        else if (this->m_range >= DEFAULT_OUT_OF_RANGE && this->m_state == STALL_ARRIVING) {
			// stall is still ARRIVING
			//this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall is ARRIVING");
			//this->logger()->log("ARRIVING: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);

		}
        else if (this->m_range >= DEFAULT_OUT_OF_RANGE && this->m_state == STALL_DEPARTING) {
			// stall is still DEPARTING
			//this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall is DEPARTING");
			//this->logger()->log("DEPARTING: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);
		}
        else if (this->m_range >= DEFAULT_OUT_OF_RANGE && this->m_state == STALL_OCCUPIED) {
			// stall is still OCCUPIED
			//this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall is OCCUPIED");
			//this->logger()->log("OCCUPIED: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);
		}

        // we are not out of range...
        else if (this->m_range < DEFAULT_OUT_OF_RANGE) {
			// just moved out of our range of interest... so we are EMPTY now...
			if (this->m_range > this->m_max_range) {
				// stall has just turned EMPTY
				this->logger()->log("ParkingStallOccupancyDetectorResource: Parking stall has just turned EMPTY");

				// send an observation only if we are DEPARTING the slot...
				if (this->m_state != STALL_EMPTY) {
					this->m_state_change = true;
				}

				// set the stall to EMPTY
				this->m_range = DEFAULT_OUT_OF_RANGE;
				this->m_last_range = DEFAULT_OUT_OF_RANGE;
				this->m_state = STALL_EMPTY;
				this->m_movement = NO_MOVEMENT;
				this->m_parking_stall_state_str = this->create_status_string(this->m_state);
				this->led_stall_empty();

				// enable observation
				this->enable_observation();

				// slot is now EMPTY
				this->led_stall_empty();

				// turn the BLE beacon off
				turn_beacon_off();

				// back to low-rez pinging
				this->m_wait_time = WAIT_TIME;
			}

			// we are within our allowed "parked" range
			else if (this->within_parking_range(this->m_range)) {
				// DEBUG
				this->logger()->log("ParkingStallOccupancyDetectorResource: stall is now OCCUPIED...");

				// state change if we are ARRIVING into the slot
				if (this->m_state != STALL_OCCUPIED) {
					this->m_state_change = true;
				}

				// set our range
				this->m_range = this->m_occupied_range;
				this->m_last_range = this->m_occupied_range;
				this->m_state = STALL_OCCUPIED;
				this->m_movement = NO_MOVEMENT;
				this->m_parking_stall_state_str = this->create_status_string(this->m_state);

				// turn the BLE beacon on
				turn_beacon_on();

				// generate an observation
				this->enable_observation();

				// slot is now OCCUPIED
				this->led_stall_occupied();

				// back to low-rez pinging
				this->m_wait_time = WAIT_TIME;
			}

			// our range lies in between our parking range and our max range... so look at the object rate change
			else {
				// DEBUG
				//this->logger()->log("ParkingStallOccupancyDetectorResource: Distance: %.2f m, state: %d move: %d",this->m_range,(int)this->m_state,(int)this->m_movement);

				// start high-rez pinging
				this->m_wait_time = HREZ_WAIT_TIME;

				//
				// object detected in range.. we must determine the stall state
				//
				// ARRIVING
				if (this->m_movement == INTO_STALL) {
					// DEBUG
					this->logger()->log("ParkingStallOccupancyDetectorResource: stall has ARRIVING car...");

					// ARRIVING
					this->m_state = STALL_ARRIVING;

					// car is ARRIVING
					this->led_stall_arriving();
				}

				// DEPARTING
				else if (this->m_movement == OUT_OF_STALL) {
					// DEBUG
					this->logger()->log("ParkingStallOccupancyDetectorResource: stall has DEPARTING car...");

					// DEPARTING
					this->m_state = STALL_DEPARTING;

					// car is DEPARTING
					this->led_stall_departing();
				}

				else {
					// car is not moving... its stationary...
					this->logger()->log("ParkingStallOccupancyDetectorResource: car is stationary...");
				}
			}
        }
        else {
			// out of range - hence EMPTY
        	this->logger()->log("ParkingStallOccupancyDetectorResource: stall is EMPTY (OUT of RANGE)");

        	// send an observation only if we are DEPARTING the slot...
			if (this->m_state != STALL_EMPTY) {
				this->m_state_change = true;
			}

			// set the stall to EMPTY
			this->m_range = DEFAULT_OUT_OF_RANGE;
			this->m_last_range = DEFAULT_OUT_OF_RANGE;
			this->m_state = STALL_EMPTY;
			this->m_movement = NO_MOVEMENT;
			this->m_parking_stall_state_str = this->create_status_string(this->m_state);
			this->led_stall_empty();

			// enable observation
			this->enable_observation();

			// slot is now EMPTY
			this->led_stall_empty();

			// turn the BLE beacon off
			turn_beacon_off();

			// back to low-rez pinging
			this->m_wait_time = WAIT_TIME;
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

// reset the observation latch
extern "C" void reset_observation_latch()  {
    __observation_latch = true;
}


#endif // __PARKING_STALL_OCCUPANCY_DETECTOR_RESOURCE_H__

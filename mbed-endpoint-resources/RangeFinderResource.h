/**
 * @file    RangeFinderResource.h
 * @brief   mbed CoAP Endpoint RangeFinder Resource
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

// Seeed ultrasound range finder
static RangeFinder __range_finder(D2, 10, 5800.0, 100000);

// Our wait time between checks for range detection (in ms)
#define WAIT_TIME                   1500    // 1.5 seconds

// Values of our resource
#define NO_OBJECT_DETECTED          "0"     // no object detected
#define OBJECT_DETECTED             "1"     // object detected

// default trigger range 
#define DEFAULT_TRIGGER_RANGE_M     1       // 1 meter

// forward declarations
static void *_instance = NULL;

// observation latch
static bool __observation_latch = true;

// reset the observation latch
extern "C" void reset_observation_latch() {
    __observation_latch = true;
}

// range finder motion detector forward reference
extern "C" void _range_detector(const void *args);

/** RangeFinderResource class
 */
class RangeFinderResource : public DynamicResource 
{
private:
    Thread         *m_range_detector;
    float           m_range;
    float           m_trigger_range;
    string          m_range_str;
    bool            m_perform_observation;

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    RangeFinderResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"RangeFinder",M2MBase::GET_PUT_ALLOWED,observable) {
        // init
        _instance = (void *)this;
        
        // initialize the range value
        this->m_range = -1.0;
        this->m_range_str = NO_OBJECT_DETECTED;
        
        // default trigger range
        this->m_trigger_range = DEFAULT_TRIGGER_RANGE_M;
        
        // no Thread yet
        this->m_range_detector = NULL;
        
        // no observation yet
        this->m_perform_observation = false;
    }

    /**
    Get the RangeFinder value
    @returns string containing base64 encoded image from the RangeFinder
    */
    virtual string get() {
        // we have to wait until the main loop starts in the endpoint before we create our thread... 
        if (this->m_range_detector == NULL) {
            // create the processing thread
            this->m_range_detector = new Thread(_range_detector,NULL,osPriorityNormal,DEFAULT_STACK_SIZE);
        }
        
        // return our latest range status
        return this->m_range_str;
    }
    
    /**
    Set the MAX Trigger range (in m)
    @param string input the string the maximum trigger range in meters
    */
    virtual void put(const string value) {
        // scan in the value
        sscanf(value.c_str(),"%f",&this->m_trigger_range);
        
        // DEBUG
        this->logger()->log("RangeFinderResource: Range trigger distance set to: %.1f m",this->m_trigger_range);
    }
    
    // call to perform an observation if needed
    void checkRange() {
        // get the latest range value
        this->get_range();
        
        // update our status
        this->update_range_status();
        
        // set our value and create an observation event
        if (this->m_perform_observation == true && __observation_latch == true) {
            this->observe();
            __observation_latch = false;
        }
    }
    
private:
    // get the latest range value
    void get_range() {
        this->m_range = __range_finder.read_m();
        
        // DEBUG
        //this->logger()->log("RangeFinderResource: Range: %.1f",this->m_range);
    }
    
    // update our status
    void update_range_status() {
        // reset our observation state
        this->m_perform_observation = false;
        
        // if thresholds crossed... record and perform an observation...
        if (this->m_range == -1.0)  {
            // ERROR in the sensor
            this->logger()->log("RangeFinderResource: Timeout Error in range finder sensor"); 
            this->m_range_str = NO_OBJECT_DETECTED;  
        } 
        else if (this->m_range > this->m_trigger_range) {  
            // nothing in range 
            //this->logger()->log("RangeFinderResource: No object within trigger range");
            this->m_range_str = NO_OBJECT_DETECTED;
        } 
        else {
            // object detected in range.. 
            this->logger()->log("RangeFinderResource: Object detected. Distance: %.1f m",this->m_range);
            this->m_range_str = OBJECT_DETECTED;
            this->m_perform_observation = true;
        }
    }
};

// range detector
void _range_detector(const void *args) {
    while (true) {
        if (_instance != NULL) {
            // check for a range-based event..
            ((RangeFinderResource *)_instance)->checkRange();
        }
        
        // wait a bit and look again...
        Thread::wait(WAIT_TIME);
    }
}

#endif // __RANGE_FINDER_RESOURCE_H__

/**
 * @file    HourGlassResource.h
 * @brief   mbed CoAP Endpoint HourGlass Resource
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

#ifndef __HOUR_GLASS_RESOURCE_H__
#define __HOUR_GLASS_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// JSON Parser
#include "MbedJSONValue.h"

// forward declarations
static void *__instance = NULL;
extern "C" void _decrementor(const void *args);

// Linkage to LCD Resource (for writing updates)
extern "C" void update_parking_meter_stats(int value, int fill_value);

// Hour Glass
static volatile bool __update_fill = false;  // trigger an update while decrementing...
static volatile bool __expired = false;      // expired parking!
static volatile int __fill_seconds = 0;      // current "fill" state to countdown to...
static volatile int __add_seconds = 0;       // number of seconds to add to current "fill" state to countdown to...
static volatile int __seconds = 0;           // current countdown value... starts at "fill" and decrements every second...

/** HourGlassResource class
 */
class HourGlassResource : public DynamicResource
{
private:
    Thread *m_countdown_thread;
    
public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the object name
    @param res_name input the resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    HourGlassResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"HourGlass",M2MBase::GET_PUT_POST_ALLOWED,observable) {
        // init
        __instance = (void *)this;
        
        // set to expired (0)
        __fill_seconds = 0;
        __seconds = 0;
        
        // no updating
        __update_fill = false;
        
        // no Thread yet
        this->m_countdown_thread = NULL;
        
        // EXPIRED
        update_parking_meter_stats(__seconds,__fill_seconds);
    }

    /**
    Get the current countdown value of the the hourglass
    @returns string representing the current value of this resource
    */
    virtual string get() {
        char buf[20];
        memset(buf,0,20);
        sprintf(buf,"%d",__seconds);
        return string(buf);
    }
    
    /**
    Put to configure the hourglass countdown fill level...
    FORMAT: {"value":60,"cmd":"update","auth":"arm1234"}
    FORMAT: {"value":60,"cmd":"set","auth":"arm1234"}
    */
    virtual void put(const string value) {
        // parameter check...
        if (value.length() > 0) {
            // parse the JSON string
            MbedJSONValue parser;
            parse(parser,value.c_str());
                        
            // Look for the "cmd" value... if it exists, follow the appropriate command...
            string cmd = parser["cmd"].get<std::string>();
            if (cmd.length() > 0) {
                // we need the authorization string
                string auth = parser["auth"].get<std::string>();
                if (strcmp(auth.c_str(),MY_DM_PASSPHRASE) == 0) {
                    // extract the hourglass value...
                    int fill_seconds = parser["value"].get<int>();

                    // DEBUG
                    this->logger()->log("HourGlassResource: put() authenticated cmd=%s fill_seconds=%d",cmd.c_str(),fill_seconds);
                    
                    // we have a authenticated command... lets parse it and act
                    if (strcmp(cmd.c_str(),"update") == 0) {
                        if (__expired == false) {
                            // make sure the change is valid (i.e. we've already set our seconds... now we are updating it...)
                            if (fill_seconds > 0 && __fill_seconds > 0) {
                                // update!
                                __fill_seconds += fill_seconds;
                                __add_seconds = fill_seconds;
                                
                                // tell the thread to update itself
                                __update_fill = true;
                                
                                // update the hourglass with a new velue
                                this->logger()->log("HourGlassResource: put() adding additional seconds: %d  total: %d",fill_seconds,__fill_seconds);
                                
                                // observe
                                //this->observe();
                            }
                            else {
                                // not updating... value unchanged or invalid
                                this->logger()->log("HourGlassResource: put() ignoring update request: current: %d requested: %d (OK).",__fill_seconds,fill_seconds);
                            }
                        }
                        else {
                            // parking is already expired... so a new "set" is required...
                            this->logger()->log("HourGlassResource: put() ignoring update request: parking timer has expired.(OK)");
                        }
                    }
                    else if (strcmp(cmd.c_str(),"set") == 0) {
                        // make sure the change is valid
                        if (fill_seconds > 0 && fill_seconds != __fill_seconds) {
                            // clean up if needed... 
                            if (__expired == true) {
                                this->reset();
                            }
                            
                            // ensure we have no outstanding decrementor thread...
                            if (this->m_countdown_thread == NULL) {
                                // set the hourglass with a new value...
                                this->logger()->log("HourGlassResource: put() setting new value: %d  last: %d",fill_seconds,__fill_seconds);
                                __fill_seconds = fill_seconds;
                            
                                // initialize...
                                this->reset();
                                
                                // observe
                                //this->observe();
                            }
                            else {
                                // current timer already active... so you cannot set it again until the timer expires
                                this->logger()->log("HourGlassResource: put() setting ignored... parking timer is active (%d sec / %d sec) (OK)",__seconds,__fill_seconds);
                            }
                        }
                        else {
                            // not resetting... value unchanged or invalid
                            this->logger()->log("HourGlassResource: put() ignoring set request: current: %d requested: %d (OK).",__fill_seconds,fill_seconds);
                        }
                    }
                    else {
                        // unrecognized command - ignore
                        this->logger()->log("HourGlassResource: put() authenticated cmd=%s is unrecognized... ignoring (OK).",cmd.c_str());
                    }
                }
                else {
                    // unauthenticated
                    this->logger()->log("HourGlassResource: put() authentication ERROR. Invalid/Missing auth: [%s]",auth.c_str());
                }
            }
            else {
                // do nothing...
                this->logger()->log("HourGlassResource: put() ignoring request (no command given) (OK).");
            }
        }
    }
    
    /**
    Post to start the countdown...
    */
    virtual void post(void *args) {
        M2MResource::M2MExecuteParameter* param = (M2MResource::M2MExecuteParameter*)args;
        if (param != NULL) {
            // use parameters
            String object_name = param->get_argument_object_name();
            String resource_name = param->get_argument_resource_name();
            string value = this->coapDataToString(param->get_argument_value(),param->get_argument_value_length());
            
            // compare to our DM passphrase... if authenticated, then begin the countdown...
            if (strcmp(value.c_str(),MY_DM_PASSPHRASE) == 0) {
                // make sure we have a timer value set...
                if (__fill_seconds > 0) {
                    // reset if we have a lingering expired thread...
                    if (this->m_countdown_thread != NULL && __expired == true) {
                        this->reset();
                    }
                    
                    // make sure we have no outstanding thread...
                    if (this->m_countdown_thread == NULL) { 
                        // reset for good measure
                        this->reset();
                        
                        // we are not expired
                        __expired = false;
                    
                        // start the decrement thread
                        this->logger()->log("HourGlassResource: post() authenticated. Starting decrement thread...");
                        this->m_countdown_thread = new Thread(_decrementor);
                    }
                    else {
                        // already running the decrement thread
                        this->logger()->log("HourGlassResource: post() authenticated. Decrement thread already running (OK)...");
                    }
                }
                else {
                    // no timer value set... so do not start the thread...
                    this->logger()->log("HourGlassResource: post() not starting decrement thread... no timer value has been set yet (OK).");
                }
            }
            else {
                // unable to authenticate 
                this->logger()->log("HourGlassResource: authentication FAILED for post(%s)",value.c_str());
            }
        }
        else {
            // unable to authenticate 
            this->logger()->log("HourGlassResource: NULL params (post() not authenticated)");
        }
        
    }
    
    // reset the countdown... clear the thread, reset the counter...
    void reset() {
        if (this->m_countdown_thread != NULL && __expired == true) {
            delete this->m_countdown_thread;
        }
        this->m_countdown_thread = NULL;
        __seconds = 0;
        __update_fill = false;
        __expired = false;
    }
};

// decrementor
void _decrementor(const void *args) {
    HourGlassResource *me = (HourGlassResource *)__instance;
    __seconds = __fill_seconds;
    
    // Countdown...
    while(__seconds >= 0) {
        update_parking_meter_stats(__seconds,__fill_seconds); // Write to LCD...
        Thread::wait(1000);                                   // wait 1 second
        __seconds -= 1;                                       // decrement 1 second
        
        // if an update occurs... update...
        if (__update_fill == true) {
            __seconds += __add_seconds;
            __update_fill = false;
            __add_seconds = 0;
        }
    }
    
    // we are done.. so zero out
    __seconds = 0;
    
    // Expired!  Observe it... you will get a "0" in the observation value... 
    if (me != NULL) {
        update_parking_meter_stats(0,__fill_seconds);
        me->observe();
        __expired = true;
    }
}

#endif // __HOUR_GLASS_RESOURCE_H__

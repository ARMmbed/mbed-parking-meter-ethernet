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

// forward declarations
static void *__instance = NULL;
extern "C" void _decrementor(const void *args);

// Linkage to LCD Resource (for writing updates)
extern "C" void clear_lcd();
extern "C" void write_to_lcd(char *buffer);

// Hour Glass
static int __fill_seconds = 0;      // current "fill" state to countdown to...
static int __seconds = 0;           // current countdown value... starts at "fill" and decrements every second...

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
        
        // no Thread yet
        this->m_countdown_thread = NULL;
        
        // default is EXPIRED
        write_to_lcd("Parking Time: EXPIRED");
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
    */
    virtual void put(const string value) {
        sscanf(value.c_str(),"%d",&__fill_seconds);
        this->reset();
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
                if (this->m_countdown_thread == NULL) {
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
        if (this->m_countdown_thread != NULL) {
            delete this->m_countdown_thread;
        }
        this->m_countdown_thread = NULL;
        __seconds = 0;
    }
};

// Display of Remaining Time to Expiration
static void note_remaining_time(int seconds) {
    char buffer[64];
    memset(buffer,0,64);
    sprintf(buffer,"Parking Time: %d secs left",seconds);
    write_to_lcd(buffer);
}    

// decrementor
void _decrementor(const void *args) {
    HourGlassResource *me = (HourGlassResource *)__instance;
    __seconds = __fill_seconds;
    
    // Countdown...
    while(__seconds > 0) {
        note_remaining_time(__seconds); // Write to LCD...
        Thread::wait(1000);             // wait 1 second
        __seconds -= 1;                 // decrement 1 second
    }
    
    // Expired!  Observe it... you will get a "0" in the observation value... 
    if (me != NULL) {
        write_to_lcd("Parking Time: EXPIRED");
        me->observe();
        me->reset();
    }
}

#endif // __HOUR_GLASS_RESOURCE_H__
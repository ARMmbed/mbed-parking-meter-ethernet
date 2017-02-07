/**
 * @file    LCDResource.h
 * @brief   mbed CoAP Endpoint LCD Resoruce
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

#ifndef __LCD_RESOURCE_H__
#define __LCD_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// String buffer for the LCD log line
#define LCD_BUFFER_LENGTH       24
static char __log[LCD_BUFFER_LENGTH+1];

// number of slots in the time remaining bar
#define NUM_SLOTS               20
static char __bar[NUM_SLOTS+1];

// Our mbed Application Shield LCD Device
#include "C12832.h"
static C12832 __lcd(D11, D13, D12, D7, D10);

// multi-color LED (must disable when using pyOCD... D8 is the debugging line...)
static PwmOut r (D5);
// static PwmOut b (D8);
static PwmOut g (D9);

// color LED RED
extern "C" void parking_status_led_red() {
    r = 0.5;
    // b = 1.0;
    g = 1.0;
}

// color LED DEFAULT
extern "C" void parking_status_led_yellow() {
    r = 0.3;
    // b = 1.0;
    g = 0.3;
}

// color LED GREEN
extern "C" void parking_status_led_green() {
    r = 1.0;
    // b = 1.0;
    g = 0.5;
}

// initialize the log buffer
extern "C" void init_log_buffer() 
{
	 memset(__log,0,LCD_BUFFER_LENGTH+1);
	 for(int i=0;i<LCD_BUFFER_LENGTH;++i) __log[i] = ' ';
}

// write to the status log line...
extern "C" void parking_meter_log_status(char *status) 
{
    if (status != NULL && strlen(status) > 0) {
        int length = strlen(status);
        if (length > LCD_BUFFER_LENGTH) {
            length = LCD_BUFFER_LENGTH;
        }
       init_log_buffer();
       for(int i=0;i<length;++i) __log[i] = status[i];
        __lcd.locate(0,20);
        __lcd.printf(__log);
    }
}

// Parking Meter Title
extern "C" void write_parking_meter_title(char *fw)
{
    //__lcd.cls();
    __lcd.locate(0,0);
    __lcd.printf("Parking Meter FW_v%s",fw);
}

// calculate the percentage completed
extern "C" double calculate_percent_remaining(int value,int fill_value)
{
    double remaining = ((value*100.0)/fill_value);
    //pc.printf("Remaining: value=%d fill_value=%d remaining=%.1f\r\n",value,fill_value,remaining);
    return remaining;
}

// generate a "bar" that represents remaining time
extern "C" char *calculate_time_remaining_bar(int value,int fill_value) 
{
    memset(__bar,0,NUM_SLOTS+1);
    double remaining = calculate_percent_remaining(value,fill_value);
    int stop_index = (int)((remaining/100.0)*NUM_SLOTS);
    for(int i=0;i<NUM_SLOTS;++i) {
    	__bar[i] = ' ';
    }
    for(int i=0;i<stop_index;++i) {
        __bar[i] = '*';
    }
    return __bar;
}

// Parking Meter Parking Time Stats Update
extern "C" void update_parking_meter_stats(int value,int fill_value)
{
    __lcd.locate(0,10);
    if (value <= 0) {
        // parking time expired
        __lcd.printf("Time: EXPIRED           ");
        parking_meter_log_status((char *)"Remain: NONE            ");
        
        // LED goes red
        parking_status_led_red();
    }
    else {
        // remaining time
        __lcd.printf("Time: %s",calculate_time_remaining_bar(value,fill_value));
        
        // use the log line too... just give the stats...
        char buf[64];
        memset(buf,0,64); 
        sprintf(buf,"Rem: %dsec / %dsec",value,fill_value);
        parking_meter_log_status(buf);
        
        // if the remaining time is less than 25% of the total, color the led YELLOW
        if (calculate_percent_remaining(value,fill_value) <= 25.0) {
            // running out of time!... maybe send a SMS...
            parking_status_led_yellow();
        }
        else {
            // parking time remaining is OK...
            parking_status_led_green();
        }
    }
}

// Parking Meter Beacon Status
extern "C" void parking_meter_beacon_status(bool enabled) 
{
    if (enabled == true) {
        parking_meter_log_status((char *)"PAID-FOR PARKING");
    }
    else {
        parking_meter_log_status((char *)"FREE PARKING");
    }
}

/** LCDResource class
 */
class LCDResource : public DynamicResource
{

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the object name
    @param res_name input the resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    LCDResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"C12832 LCD",M2MBase::GET_PUT_ALLOWED,observable) {
       init_log_buffer();
    }

    /**
    Get the value of the the LCD's log line text buffer
    @returns string representing the current value of this resource
    */
    virtual string get() {
        this->logger()->log("C12832 LCD: GET() called. Log buffer: %s",__log);
        return string(__log);
    }
    
    /**
    Put to write to the LCD (formatting and "\r\n" chars must be part of the text in the put() command)
    */
    virtual void put(const string value) {
        this->logger()->log("C12832 LCD: PUT(%s) called",value.c_str());
        parking_meter_log_status((char *)value.c_str());
    }
};

#endif // __LCD_RESOURCE_H__

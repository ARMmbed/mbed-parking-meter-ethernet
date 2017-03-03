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

// JSON parser
#include "MbedJSONValue.h"

// String buffer for the LCD log line
#define LCD_BUFFER_LENGTH       24
static char __log[LCD_BUFFER_LENGTH+1];

// number of slots in the time remaining bar
#define NUM_SLOTS               20
static char __bar[NUM_SLOTS+1];

#if ENABLE_V2_RESOURCES
// shield uses the

// 4 LEDs used
DigitalOut green_led(D7);
DigitalOut blue_led(D6);
DigitalOut yellow_led(D5);
DigitalOut red_led(D4);

// LCD
#include "SB1602E.h"
SB1602E __lcd(D14,D15);    //  SDA, SCL
#else
// Our mbed Application Shield LCD Device
#include "C12832.h"
static C12832 __lcd(D11, D13, D12, D7, D10);

// multi-color LED (must disable when using pyOCD... D8 is the debugging line...)
static PwmOut r (D5);
//static PwmOut b (D8);
static PwmOut g (D9);
#endif

#if ENABLE_V2_RESOURCES
extern Logger logger;
// RED LED on/off
extern "C" void parking_status_led_red(bool on) {
	if (on) {
		//logger.log("RED LED ON");
		//red_led = 0;
	}
	else {
		//logger.log("RED LED OFF");
		//red_led = 1;
	}
}
extern "C" void parking_status_led_yellow(bool on) {
	if (on) {
		//logger.log("YELLOW LED ON");
		//yellow_led = 0;
	}
	else {
		//logger.log("YELLOW LED OFF");
		//yellow_led = 1;
	}
}
extern "C" void parking_status_led_green(bool on) {
	if (on) {
		//logger.log("GREEN LED ON");
		//green_led = 0;
	}
	else {
		//logger.log("GREEN LED OFF");
		//green_led = 1;
	}
}
extern "C" void parking_status_led_blue(bool on) {
	if (on) {
		//logger.log("BLUE LED ON");
		//blue_led = 0;
	}
	else {
		//logger.log("BLUE LED OFF");
		//blue_led = 1;
	}
}
#else
// color LED RED
extern "C" void parking_status_led_red(bool on) {
	if (on) {
		r = 0.5;
		//b = 1.0;
		g = 1.0;
	}
	else {
		r = 1.0;
		//b = 1.0;
		g = 1.0;
	}
}

// color LED DEFAULT
extern "C" void parking_status_led_yellow(bool on) {
	if (on) {
		r = 0.3;
		//b = 1.0;
		g = 0.3;
	}
	else {
		r = 1.0;
		//b = 1.0;
		g = 1.0;
	}
}

// color LED GREEN
extern "C" void parking_status_led_green(bool on) {
	if (on) {
		r = 1.0;
		//b = 1.0;
		g = 0.5;
	}
	else {
		r = 1.0;
		//b = 1.0;
		g = 1.0;
	}
}

// color LED GREEN
extern "C" void parking_status_led_blue(bool on) {
    if (on) {
		r = 1.0;
		//b = 0.5;
		g = 1.0;
    }
    else {
    	r = 1.0;
		//b = 0.5;
		g = 1.0;
    }
}
#endif // ENABLE_V2_RESOURCES

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
#if ENABLE_V2_RESOURCES
        __lcd.puts(1,(char *)__log);
#else
        __lcd.locate(0,20);
        __lcd.printf(__log);
#endif
    }
}

// Parking Meter Title
extern "C" void write_parking_meter_title(char *fw)
{
#if ENABLE_V2_RESOURCES
	__lcd.clear();
	char buf[96];
	memset(buf,0,96);
	sprintf(buf,"Parking Meter FW_v%s\r",fw);
	__lcd.puts(0,(char *)buf);
#else
    //__lcd.cls();
    __lcd.locate(0,0);
    __lcd.printf("Parking Meter FW_v%s",fw);
#endif
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
#if ENABLE_V2_RESOURCES
	// XXX TO DO
#else
    __lcd.locate(0,10);
    if (value <= 0) {
        // parking time expired
        __lcd.printf("Time: EXPIRED           ");
        parking_meter_log_status((char *)"Remain: NONE            ");
        
        // LED goes red
        parking_status_led_red(true);
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
            parking_status_led_yellow(true);
        }
        else {
            // parking time remaining is OK...
            parking_status_led_green(true);
        }
    }
#endif
}

// Parking Meter Beacon Status
extern "C" void parking_meter_beacon_status(int status) 
{
    if (status == 0) {
        parking_meter_log_status((char *)"FREE PARKING");
    }
    else if (status == 2) {
        parking_meter_log_status((char *)"BEACON-OFF");
    }
    else if (status == 1) {
        parking_meter_log_status((char *)"PAID-FOR PARKING");
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
    LCDResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"LCD",M2MBase::GET_PUT_ALLOWED,observable) {
       init_log_buffer();
#if ENABLE_V2_RESOURCES
       __lcd.contrast(0x35);
#endif
    }

    /**
    Get the value of the the LCD's log line text buffer
    @returns string representing the current value of this resource
    */
    virtual string get() {
        this->logger()->log("Log: %s",__log);
        return string(__log);
    }
    
    /**
    Put to write to the LCD (formatting and "\r\n" chars must be part of the text in the put() command)
    Format: {"cmd":"lcd|led","value":"text|red|blue|green","state":0|1}
    */
    virtual void put(const string value) {
    	// parse the JSON
    	MbedJSONValue parsed;
    	parse(parsed,value.c_str());

    	// get the command type
    	string cmd = parsed["cmd"].get<string>();
    	string val = parsed["value"].get<string>();

    	// act on the command
    	if (cmd.compare(string("lcd")) == 0) {
    		// write to the LCD
    		//this->logger()->log("PUT(%s) called",val.c_str());
    		parking_meter_log_status((char *)val.c_str());
    	}
    	if (cmd.compare(string("led")) == 0) {
    		// get the state value
    		int state = parsed["state"].get<int>();
    		bool bool_state = false;
    		if (state) bool_state = true;

    		// toggle based on state
			if (val.compare(string("red")) == 0) {
				parking_status_led_red(bool_state);
			}
			if (val.compare(string("yellow")) == 0) {
				parking_status_led_yellow(bool_state);
			}
			if (val.compare(string("green")) == 0) {
				parking_status_led_green(bool_state);
			}
			if (val.compare(string("blue")) == 0) {
				parking_status_led_blue(bool_state);
			}
    	}
    }
};

#endif // __LCD_RESOURCE_H__

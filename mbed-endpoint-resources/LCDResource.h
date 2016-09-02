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

// String buffer for the LCD panel
#define LCD_BUFFER_LENGTH       128
static char __lcd_buffer[LCD_BUFFER_LENGTH+1];

// Our mbed Application Shield LCD Device
#include "C12832.h"
static C12832 __lcd(D11, D13, D12, D7, D10);

// clear the LCD
extern "C" void clear_lcd() {
    __lcd.cls();
    __lcd.locate(0,0);
}

// write to LCD
extern "C" void write_to_lcd(char *buffer) {
    clear_lcd();
    __lcd.printf("%s",buffer);
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
    LCDResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false) : DynamicResource(logger,obj_name,res_name,"C12832 LCD",M2MBase::GET_PUT_POST_ALLOWED,observable) {
        memset(__lcd_buffer,0,LCD_BUFFER_LENGTH+1);
        sprintf(__lcd_buffer,"LCD: %s/*/%s Active",obj_name,res_name);
        write_to_lcd(__lcd_buffer);
    }

    /**
    Get the value of the the LCD's text buffer
    @returns string representing the current value of this resource
    */
    virtual string get() {
        this->logger()->log("C12832 LCD: GET() called. Buffer: %s",__lcd_buffer);
        return string(__lcd_buffer);
    }
    
    /**
    Put to write to the LCD (formatting and "\r\n" chars must be part of the text in the put() command)
    */
    virtual void put(const string value) {
        this->logger()->log("C12832 LCD: PUT(%s) called",value.c_str());
        int length = value.length();
        if (length > LCD_BUFFER_LENGTH) {
            length = LCD_BUFFER_LENGTH;
        }
        memset(__lcd_buffer,0,LCD_BUFFER_LENGTH+1);
        strncpy(__lcd_buffer,value.c_str(),length);
        write_to_lcd(__lcd_buffer);
    }
    
    /**
    Post to clear the LCD panel...
    */
    virtual void post(void *args) {
        // clear the LCD
        this->logger()->log("C12832 LCD: POST() called. Clearing LCD...");
        clear_lcd();
    }
};

#endif // __LCD_RESOURCE_H__


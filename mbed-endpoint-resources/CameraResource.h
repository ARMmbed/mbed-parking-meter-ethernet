/**
 * @file    CameraResource.h
 * @brief   mbed CoAP Endpoint Camera Resource
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

#ifndef __CAMERA_RESOURCE_H__
#define __CAMERA_RESOURCE_H__

// Base class
#include "mbed-connector-interface/DynamicResource.h"

// Camera Resource
#include "CameraOV528.h"
static CameraOV528 __camera(A3,A2);

// Base64 encoder/decoder
#include "Base64.h"

// maximum supported image size
#define MAX_CAMERA_BUFFER_SIZE              1660         // ~1.7k jpeg for image resolution 160x120
#define MAX_MESSAGE_SIZE                    1023         // CoAP limits to 1024 - max message length 

// RangeFinder Observation Latch reset
extern "C" void reset_observation_latch();

/** CameraResource class
 */
class CameraResource : public DynamicResource
{
private:
    uint8_t         m_camera_buffer[MAX_CAMERA_BUFFER_SIZE+1];
    uint32_t        m_camera_buffer_length;
    string          m_image;
    Base64          m_base64;
    Authenticator  *m_authenticator;

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    CameraResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false,Authenticator *authenticator = NULL) : DynamicResource(logger,obj_name,res_name,"Camera",M2MBase::GET_POST_ALLOWED,observable) {
        this->m_authenticator = authenticator;
        memset(this->m_camera_buffer,0,MAX_CAMERA_BUFFER_SIZE+1);   
        this->m_camera_buffer_length = 0;
        this->m_image = "";
        this->init_camera();
    }

    /**
    Get the Camera's last taken picture
    @returns string containing base64 encoded image from the camera
    */
    virtual string get() { 
        // return the last image we have taken...
        return this->m_image;
    }
    
    /**
    POST: Take a picture with the camera (AUTHENTICATED)
    */
    virtual void post(void *args) {
        if (this->authenticate(args)) {
            // authenticatd
            this->logger()->log("CameraResource: POST authenticated successfully...");
            
            // take a picture
            this->logger()->log("CameraResource: Taking a picture...");
            this->take_picture();
            
            // encode the image
            this->logger()->log("CameraResource: Encoding picture...");
            this->encode_image();
            
            // create the observation
            this->logger()->log("CameraResource: Creating observation...");
            this->observe();
            
            // now that the image has been observed... release the RangeFinder observation latch
            reset_observation_latch();
        }
        else {
            // authentication failed
            this->logger()->log("CameraResource: Not taking picture. Authentication FAILED.");
        }
    }

private:
    // authenticate
    bool authenticate(const void *challenge) {
        if (this->m_authenticator != NULL) {
            return this->m_authenticator->authenticate((void *)challenge);
        }
        return false;
    }

    // take a picture
    void take_picture() {         
        // take a picture
        __camera.take_picture();
        
        // transfer the picture
        this->transfer_picture();
    }
    
    // initialize the camera
    void init_camera() {
        // initialize the camera
        __camera.powerup();
        
        // set the resolution
        __camera.set_resolution(CameraOV528::RES_160x120);
        
        // set the format
        __camera.set_format(CameraOV528::FMT_JPEG);
    }
    
    // transfer the camera picture
    void transfer_picture() {
        // clear the buffer
        memset(this->m_camera_buffer,0,MAX_CAMERA_BUFFER_SIZE+1);
        this->m_camera_buffer_length = 0;
        
        // get the buffer size - clip to the maximum buffer size available...
        uint32_t buffer_size = __camera.get_picture_size();
        if (buffer_size > MAX_CAMERA_BUFFER_SIZE) {
            buffer_size = MAX_CAMERA_BUFFER_SIZE;
        }
        
        // read in the picture...
        if (buffer_size > 0) {
            // read in the image
            this->m_camera_buffer_length = __camera.read_picture_data(this->m_camera_buffer,buffer_size);
        }
    }
    
    // encode the image into base64
    void encode_image() {
        size_t buf_length = 0; 
        
        // make sure that the buffer we have has stuff in it...
        if (this->m_camera_buffer_length > 0) {
            // ability to clip part of the image...    
            int clip_length = this->m_camera_buffer_length;
            if (clip_length > MAX_MESSAGE_SIZE) {
                // Base64 will add some to the length... so trim a bit more back (256 bytes)
                clip_length = MAX_MESSAGE_SIZE - 256;
                
                // DEBUG
                clip_length = 50;
                
                // DEBUG
                this->logger()->log("CameraResource: Image length: %d too big for CoAP... trimming to: %d bytes...",this->m_camera_buffer_length,clip_length);
            }
            
            // Base64 Encode
            char *buf = this->m_base64.Encode((const char *)this->m_camera_buffer,clip_length,&buf_length);
            if (buf != NULL && buf_length > 0) {
                // copy to string
                this->m_image = buf;
                
                // Base64.Encode malloc()'d the buffer... so free() it
                if (buf != NULL) {
                    free(buf);
                }
            }
        }
        else {
            this->logger()->log("CameraResource: empty image");
            this->m_image = "";
        }
        
        // DEBUG
        this->logger()->log("CameraResource: Base64 message length: %d",strlen(this->m_image.c_str()));
    }
};

#endif // __CAMERA_RESOURCE_H__

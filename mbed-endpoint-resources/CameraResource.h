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

// our instance
void *_camera_instance = NULL;

// Base64 encoder/decoder
#include "Base64.h"

// Gzip utils
#include "gzip_utils.h"

// TUNE: buffer sizes
#define MAX_CAMERA_BUFFER_SIZE              5192         // ~5k jpeg for image resolution 160x120... plus some wiggle room...
#define MAX_MESSAGE_SIZE                    1024         // CoAP limits to 1024 - max message length
#define PREFERRED_MESSAGE_LEN				225	         // preferred "chunk" size for a single observation

// TUNE: how long we wait between sending image observations
#define WAIT_BETWEEN_OBSERVATIONS_MS		750	     	 // # of seconds (in ms) to wait between sending observations

// OPTION: use a Thread to dispatch the observations
#define USE_THREADING						true		 // true: a dispatch thread will send observations, false: post() execution will send observations

// OPTION: Enable/Disable Base64 encode of image
#define DO_BASE64_ENCODE_IMAGE				true		 // true: base64 encode of final image data, false - send as raw data (may break!)

// OPTION: Enable/Disable GZIP of image
#define DO_GZIP_IMAGE						false		 // true: gzip prior to base64 encoding... false: just base64 encode raw JPEG

// OPTION: Clip message
#define DO_CLIP_MESSAGE						false		 // set to true to clip an image...
#define CLIP_LENGTH			    			512			 // Clip length to clip image such that the CoAP message is small enough...

// END delimiter - this will be checked in the NodeRED flow to initiate the "join" node to combine the image segments
#define END_DELIMITER						"END"

// Thumbnail debugging length
#define THUMBNAIL_LEN						20

// RangeFinder Observation Latch reset
extern "C" void reset_observation_latch();

// observation processor forward reference
extern "C" void _process_observations(const void *args);

// temp buffer
static uint8_t tmp_buffer[MAX_CAMERA_BUFFER_SIZE+1];

/** CameraResource class
 */
class CameraResource : public DynamicResource
{
private:
	Thread         *m_observer;
    uint8_t         m_camera_buffer[MAX_CAMERA_BUFFER_SIZE+1];
    uint32_t        m_camera_buffer_length;
    string          m_image;
    vector<string>  m_image_list;
    int			    m_image_list_index;
    Base64          m_base64;
    Authenticator  *m_authenticator;
    string 			m_end;

public:
    /**
    Default constructor
    @param logger input logger instance for this resource
    @param obj_name input the Light Object name
    @param res_name input the Light Resource name
    @param observable input the resource is Observable (default: FALSE)
    */
    CameraResource(const Logger *logger,const char *obj_name,const char *res_name,const bool observable = false,Authenticator *authenticator = NULL) : DynamicResource(logger,obj_name,res_name,"Camera",M2MBase::GET_POST_ALLOWED,observable) {
        _camera_instance = (void *)this;
    	this->m_authenticator = authenticator;
        memset(this->m_camera_buffer,0,MAX_CAMERA_BUFFER_SIZE+1);   
        this->m_camera_buffer_length = 0;
        this->m_image = "";
        this->m_image_list.empty();
        this->m_image_list_index = -1;
        this->init_camera();
        this->m_observer = NULL;
        this->m_end = END_DELIMITER;
    }

    /**
    Get the Camera's last taken picture
    @returns string containing base64 encoded image from the camera
    */
    virtual string get() { 
        // return the last image we have taken...
    	if (this->m_image.compare(this->m_end) != 0 && this->m_image_list_index >= 0 && this->m_image_list.size() > 0 && this->m_image_list_index < (int)this->m_image_list.size()) {
    		// DEBUG
    		int last = this->m_image_list[this->m_image_list_index].size() - THUMBNAIL_LEN;
    		this->logger()->log("CameraResource: GET(%d): (%d bytes): %s...%s",this->m_image_list_index,this->m_image_list[this->m_image_list_index].size(),this->m_image_list[this->m_image_list_index].substr(0,THUMBNAIL_LEN).c_str(),this->m_image_list[this->m_image_list_index].substr(last,THUMBNAIL_LEN).c_str());

    		// return the current string from our array of strings
    		return this->m_image_list[this->m_image_list_index];
    	}

    	// default is to just return this image...
    	this->logger()->log("CameraResource: GET: (%d bytes): %s",this->m_image.size(),this->m_image.c_str());
        return this->m_image;
    }
    
    /**
    POST: Take a picture with the camera (AUTHENTICATED)
    */
    virtual void post(void *args) {
        if (this->authenticate(args)) {
        	// authenticatd
        	this->logger()->log("CameraResource: POST authenticated successfully...");

    		// reset observation state
    		this->resetObservationState();

    		// take a picture
    		this->logger()->log("CameraResource: Taking a picture...");
    		this->take_picture();

            // start a new observer thread
            if (USE_THREADING && this->m_observer == NULL) {
            	// launch a thread that will invoke process_observations()...
            	this->logger()->log("CameraResource: Starting observation processing thread...");
            	this->m_observer = new Thread(_process_observations,NULL,osPriorityNormal,DEFAULT_STACK_SIZE);
            }
            else if (this->m_observer == NULL) {
            	// call directly...
            	this->logger()->log("CameraResource: calling process_observations() directly...");
            	this->process_observations();
            }
            else {
            	// ERROR
            	this->logger()->log("CameraResource: observation process thread lingering (ERROR)");
            }
        }
        else {
            // authentication failed
            this->logger()->log("CameraResource: Not taking picture. Authentication FAILED.");
        }
    }

    // process observations: split an image into suitable CoAP messages and create "n" observations with it
	void process_observations() {
		// encode the image
		this->logger()->log("CameraResource: Base64 encoding picture...");
		this->encode_image();

		// wait a bit...
		this->logger()->log("CameraResource: waiting a bit...");
		Thread::wait(2*WAIT_BETWEEN_OBSERVATIONS_MS);

		// get the number of chunks we have to make
		int num_observations = this->calculateNumberOfObservations(PREFERRED_MESSAGE_LEN);
		if (num_observations > 0) {
			// loop through and send each observation
			for(int i=0;i<num_observations;++i) {
				// set current the ith observation
				this->setCurrentObservation(i);

				// create/send the observation
				this->observe();

				// wait a bit
				Thread::wait(WAIT_BETWEEN_OBSERVATIONS_MS);
			}
		}
		else {
			// image is empty... no observations made
			this->logger()->log("CameraResource: Image is emnpty... no observations made (OK).");
		}

		// wait a bit
		Thread::wait(WAIT_BETWEEN_OBSERVATIONS_MS);

		// send the end message - this will invoke processing on the image...
		this->send_end_observation();

		// now that the image has been observed... release the RangeFinder observation latch
		reset_observation_latch();
	}

private:
    // authenticate
    bool authenticate(const void *challenge) {
        if (this->m_authenticator != NULL) {
            return this->m_authenticator->authenticate((void *)challenge);
        }
        return false;
    }

    // send the "END" observation
    void send_end_observation() {
    	this->logger()->log("CameraResource: Sending END observation...");
    	this->m_image = END_DELIMITER;
    	this->observe();
    }

    // calculate the number of observations needed
    int calculateNumberOfObservations(int preferred_msg_length) {
    	// split the string into a vector<string> array
    	this->m_image_list = this->split_string(this->m_image,preferred_msg_length);

    	// get the array length
    	int count = this->m_image_list.size();

    	// DEBUG
    	this->logger()->log("CameraResource: Number of observations to send (including END obs): %d",count+1);

    	// return the array length
    	return count;
    }

    // split our string into a vector<string> each with a specific length
    vector<string> split_string(string str,int length) {
    	vector<string> strings;
    	for (int i=0;i<str.length();i+=length) {
    		strings.push_back(str.substr(i,length));
    	}
    	return strings;
    }

    // set the ith observation as current
    void setCurrentObservation(int index) {
    	this->m_image_list_index = index;

    	// DEBUG
		int last = this->m_image_list[this->m_image_list_index].size() - THUMBNAIL_LEN;
		this->logger()->log("CameraResource: %dth CoAP observation (%d bytes) begin: %s end: %s...",this->m_image_list_index+1,this->m_image_list[this->m_image_list_index].size(),this->m_image_list[this->m_image_list_index].substr(0,THUMBNAIL_LEN).c_str(),this->m_image_list[this->m_image_list_index].substr(last,THUMBNAIL_LEN).c_str());
    }

    // reset any observation state
    void resetObservationState() {
    	// kill any previous observe thread
		if (this->m_observer != NULL) {
			delete this->m_observer;
		}
		this->m_observer = NULL;
		this->m_image_list.empty();
		this->m_image = "";
		this->m_image_list_index = -1;
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

		// clear our tmp buffer
		memset(tmp_buffer,0,MAX_CAMERA_BUFFER_SIZE+1);
        
        // get the buffer size - clip to the maximum buffer size available...
        uint32_t buffer_size = __camera.get_picture_size();
        if (buffer_size > MAX_CAMERA_BUFFER_SIZE) {
            buffer_size = MAX_CAMERA_BUFFER_SIZE;
        }
        
        // read in the picture...
        if (buffer_size > 0) {
            // read in the image
            int tmp_buffer_length = __camera.read_picture_data(tmp_buffer,buffer_size);

			// DEBUG
			this->logger()->log("CameraResource: RAW jpeg camera buffer size: %d (ret: %d)",buffer_size,tmp_buffer_length);

			// DEBUG
			char *image_type = "JPEG";

			// check if we want to gzip up the jpeg...
			if (DO_GZIP_IMAGE) {
				// gzip the image
				image_type = "GZIP_JPEG";
				unsigned long gzip_length = MAX_CAMERA_BUFFER_SIZE;
				int gzip_status = gzip((unsigned char *)this->m_camera_buffer,&gzip_length,(unsigned char *)tmp_buffer,(unsigned long)tmp_buffer_length);
				if (gzip_status == Z_OK) {
					// update to the gzipped length
					this->m_camera_buffer_length = (uint32_t)gzip_length;
				}
				else {
					// ERROR
					this->m_camera_buffer_length = 0;
					memset(this->m_camera_buffer,0,MAX_CAMERA_BUFFER_SIZE+1);
					this->logger()->log("CameraResource: ERROR GZIP failed: %d",gzip_status);
				}
			}
			else {
				// do not gzip the image
				this->m_camera_buffer_length = tmp_buffer_length;
				memcpy(this->m_camera_buffer,tmp_buffer,tmp_buffer_length);
			}

			// DEBUG
			this->logger()->log("CameraResource: %s camera buffer size: %d",image_type,this->m_camera_buffer_length);
        }
    }
    
    // encode the image into base64
    void encode_image() {
        // make sure that the buffer we have has stuff in it...
        if (this->m_camera_buffer_length > 0) {
            // OPTION: ability to clip part of the image...
            int clip_length = this->m_camera_buffer_length;
            if (DO_CLIP_MESSAGE && clip_length > MAX_MESSAGE_SIZE) {
                // Base64 will add some to the length... so trim a bit more back (CLIP_LENGTH bytes)
                clip_length = MAX_MESSAGE_SIZE - CLIP_LENGTH;
                
                // DEBUG
                this->logger()->log("CameraResource: Image length: %d too big for CoAP... trimming to: %d bytes...",this->m_camera_buffer_length,clip_length);
            }
            
            // OPTION: Base64 Encode
            if (DO_BASE64_ENCODE_IMAGE) {
            	size_t buf_length = 0;
				char *buf = this->m_base64.Encode((const char *)this->m_camera_buffer,clip_length,&buf_length);
				if (buf != NULL && buf_length > 0) {
					// copy to string
					this->m_image = buf;

					// DEBUG
					int last = this->m_image.size() - THUMBNAIL_LEN;
					this->logger()->log("CameraResource: Base64 encoded: length: %d begin: %s end: %s",this->m_image.size(),this->m_image.substr(0,THUMBNAIL_LEN).c_str(),this->m_image.substr(last,THUMBNAIL_LEN).c_str());

					// Base64.Encode malloc()'d the buffer... so free() it
					if (buf != NULL) {
						free(buf);
					}
				}
            }
            else {
            	// raw string copy (may not work given control characters in the binary stream)
            	this->logger()->log("CameraResource: NOTE: not base64 encoding raw image data... control characters my break things...");
            	this->m_image = (const char *)this->m_camera_buffer;
            	this->logger()->log("CameraResource: Raw string length: %d buffer length: %d",this->m_image.size(),this->m_camera_buffer_length);
            }
        }
        else {
        	// image is empty...
            this->logger()->log("CameraResource: empty image");
            this->m_image = "";
        }
        
        // DEBUG
        this->logger()->log("CameraResource: Base64 message length: %d",strlen(this->m_image.c_str()));
    }
};

// observation processor
extern "C" void _process_observations(const void *args) {
	if (_camera_instance != NULL) {
		// process the observations for the camera
		((CameraResource *)_camera_instance)->process_observations();
	}
}

#endif // __CAMERA_RESOURCE_H__


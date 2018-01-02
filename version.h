#ifndef __VERSION_H__
#define __VERSION_H__

// Define our firmware version (toggle for FOTA demonstration)
#define PKM_FW_VERSION                                   "v1"
//#define PKM_FW_VERSION                                 "v2"

// Define our hardware version
#define PKM_HW_VERSION                                   "v4"

// Enable v2 of the endpoint
#define ENABLE_V2_RESOURCES		   		  true 

// Enable CAMERA
#if ENABLE_V2_RESOURCES
	#define ENABLE_V2_CAMERA			  true
	#define ENABLE_V2_OCCUPANCY_DETECTOR              true
	#define ENABLE_V2_COMPAT			  false
#endif

#endif // __VERSION_H__

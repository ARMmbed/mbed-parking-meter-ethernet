#ifndef __TIME_UTILS_H__
#define __TIME_UTILS_H__

// mbed support
#include "mbed.h"

// NTP library support
#include "ntp-client/NTPClient.h"

// initialize time for the endpoint
extern "C" init_time(void);

#endif // __TIME_UTILS_H__

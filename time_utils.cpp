// includes
#include "time_utils.h"

// Customizable defines
#define TIME_FORMAT_STR         "%F,%H:%M:%S,%Z"	 
#define DEF_TIME_STR		"1970-01-01,00:00:00,GMT"
#define TIME_STR_BUFFER_LEN	64

// Serial support
extern Serial pc;

// NTP support for Time
static NTPClient *ntp = NULL;

// string buffer for displaying time
static char time_str_buf[TIME_STR_BUFFER_LEN+1];

// initialize our time
extern "C" void init_time(void){
    extern NetworkInterface *__network_interface;
    bool time_set = false;
    if (ntp == NULL) {
        // allocate the NTP client
        ntp = new NTPClient(__network_interface);
        for(int i=0;ntp != NULL && i<NTP_NUM_TRIES && time_set == false;++i) {
            // get the current time
            time_t current_time = ntp->get_timestamp();
            if (current_time > 0) {
                // set the current time in the RTC
                set_time(current_time);
                time_set = true;
            }
        }

        // DEBUG
        if (time_set == false) {
            pc.printf("ERROR: Unable to capture current time from NTP...Please reboot\r\n");
        }
        else {
            time_t utc_now = time(NULL);
	    memset(time_str_buf,0,TIME_STR_BUFFER_LEN+1);
	    strftime(time_str_buf,TIME_STR_BUFFER_LEN,TIME_FORMAT_STR,localtime(&utc_now));
            pc.printf("INFO: Current UTC time: %s\r\n",time_str_buf);
        }
    }
}

// time utils are initialized?
extern "C" bool time_utils_initialized() {
   return (ntp != NULL);
}

// get the current time
extern "C" char *get_current_time(void) {
    memset(time_str_buf,0,TIME_STR_BUFFER_LEN+1);
    strcpy(time_str_buf,DEF_TIME_STR);
    if (time_utils_initialized()) {
        // current time
        time_t utc_now = time(NULL);

        // convert to string format
        memset(time_str_buf,0,TIME_STR_BUFFER_LEN+1);
        strftime(time_str_buf,TIME_STR_BUFFER_LEN,TIME_FORMAT_STR,localtime(&utc_now));
    }
    return time_str_buf;
}

// includes
#include "time_utils.h"

// Serial suppot
extern Serial pc;

// establish Time
static NTPClient *ntp = NULL;

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
            pc.printf("INFO: Current NTP time: %lu\r\n",time(NULL));
        }
    }
}

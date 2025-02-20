#ifndef CLOCK_H
#define CLOCK_H

#include <stdio.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"


bool obtain_time();
void time_sync_notification_cb(struct timeval *tv);
#endif //CLOCK_H
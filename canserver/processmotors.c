/*
 * This file is part of the CANserver project.
 * Copyright 2020 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aux.h"
#include "canbus.h"
#include "canopen.h"
#include "cmdlnopts.h"
#include "processmotors.h"
#include "pusirobot.h"
#include "socket.h"

#include <fcntl.h>      // open
#include <stdio.h>      // printf
#include <string.h>     // strcmp
#include <sys/stat.h>   // open
#include <unistd.h>     // usleep
#include <usefull_macros.h>

message CANbusMessages = {0}; // CANserver thread is master

static void *stpemulator(void *arg);

// handlers for standard types
thread_handler CANhandlers[] = {
    {"emulation", stpemulator},
    {NULL, NULL}
};

thread_handler *get_handler(const char *name){
    for(thread_handler *ret = CANhandlers; ret->name; ++ret){
        if(strcmp(ret->name, name)) continue;
        return ret;
    }
    return NULL;
}

/**
 * @brief CANserver - main CAN thread; receive/transmit raw messages by CANbusMessages
 * @param data - unused
 * @return unused
 */
void *CANserver(_U_ void *data){
    char *devname = find_device();
    if(!devname){
        LOGERR("Can't find serial device");
        ERRX("Can't find serial device");
    }
    while(1){
        int fd = open(devname, O_RDONLY);
        if(fd == -1){
            WARN("open()");
            LOGWARN("Device %s is absent", devname);
            FREE(devname);
            double t0 = dtime();
            while(dtime() - t0 < 5.){
                if((devname = find_device())) break;
                usleep(1000);
            }
            if(!devname){
                LOGERR("Can't open serial device, kill myself");
                ERRX("Can't open device, kill myself");
            }else LOGMSG("Change device to %s", devname);
        }else close(fd);

        char *mesg = getmesg(idxMISO, &CANbusMessages);
        if(mesg){
            DBG("Received message: %s", mesg);
            FREE(mesg);
            // global messages to all clients:
            addmesg(idxMISO, &ServerMessages, "CANserver works\n");
        }
    }
    LOGERR("CANserver(): UNREACHABLE CODE REACHED!");
    return NULL;
}


static void *stpemulator(void *arg){
    threadinfo *ti = (threadinfo*)arg;
    while(1){
        char *mesg = getmesg(idxMISO, &ti->mesg);
        if(mesg){
            DBG("Stepper emulator got: %s", mesg);
            addmesg(idxMISO, &ServerMessages, mesg);
            /* do something */
            FREE(mesg);
        }
        int r100 = rand() % 10000;
        if(r100 < 20){ // 20% of probability
            addmesg(idxMISO, &ServerMessages, "stpemulator works fine!\n");
        }
        if(r100 > 9998){
            addmesg(idxMISO, &ServerMessages, "O that's good!\n");
        }
        usleep(1000);
    }
    LOGERR("stpemulator(): UNREACHABLE CODE REACHED!");
    return NULL;
}

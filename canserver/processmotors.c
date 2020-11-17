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

static int CANspeed = 0; // default speed, if !=0 set it when connected

// all messages are in format "ID [data]"
message CANbusMessages = {0}; // CANserver thread is master

// basic threads
// messages: master - thread, slave - caller
static void *stpemulator(void *arg);
static void *rawcommands(void *arg);
static void *canopencmds(void *arg);

// handlers for standard types
thread_handler CANhandlers[] = {
    {"emulation", stpemulator},
    {"raw", rawcommands},
    {"canopen", canopencmds},
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
 * @brief parsePacket - convert text into can packet data
 * @param packet (o) - pointer to CANpacket or NULL (just to check)
 * @param data - text in format "ID [byte0 ... byteN]"
 * @return 0 if all OK
 */
static int parsePacket(CANmesg *packet, char *data){
    if(!data || *data == 0){ // no data
        return 1;
    }
    long info[10]={0};
    int N = 0;
    char *saveptr = NULL;
    for(char *s = data; N < 10; s = NULL, ++N){
        char *nxt = strtok_r(s, " \t,;\r\n", &saveptr);
        if(!nxt) break;
        if(str2long(nxt, &info[N])) break;
    }
    if(N > 9 || N == 0) return 1; // ID + >8  data bytes or no at all
    if(packet){
        packet->ID = info[0];
        packet->len = N - 1;
    }
    for(int i = 1; i < N; ++i){
        if(info[i] < 0 || info[i] > 0xff) return 2;
        if(packet) packet->data[i-1] = (uint8_t) info[i];
    }
    return 0;
}

// [re]open serial device
static void reopen_device(){
    char *devname = NULL;
    double t0 = dtime();
    canbus_close();
    DBG("Try to [re]open serial device");
    while(dtime() - t0 < 5.){
        if((devname = find_device())) break;
        usleep(1000);
    }
    if(!devname || canbus_open(devname)){
        FREE(devname);
        LOGERR("Can't find serial device");
        ERRX("Can't find serial device");
    }else {DBG("Opened device: %s", devname);}
    if(CANspeed){ // set default speed
        canbus_clear();
        canbus_setspeed(CANspeed);
        canbus_clear();
    }
    FREE(devname);
}

// do something with can message: send to receiver
static void processCANmessage(CANmesg *mesg){
    threadinfo *ti = findThreadByID(mesg->ID);
    if(!ti) return;
    DBG("Found");
    char buf[64], *ptr = buf;
    int l = 64, x;
    x = snprintf(ptr, l, "#0x%03X ", mesg->ID);
    l -= x; ptr += x;
    for(int i = 0; i < mesg->len; ++i){
        x = snprintf(ptr, l, "0x%02X ", mesg->data[i]);
        l -= x; ptr += x;
    }
    addmesg(&ti->answers, buf);
}

/**
 * @brief CANserver - main CAN thread; receive/transmit raw messages by CANbusMessages
 * @param data - unused
 * @return unused
 */
void *CANserver(_U_ void *data){
    reopen_device();
    while(1){
        CANmesg cm;
        char *mesg = getmesg(&CANbusMessages);
        if(mesg){
            if(parsePacket(&cm, mesg)){
                LOGMSG("Received wrong CAN message: %s", mesg);
                DBG("Bad message: %s", mesg);
            }else{
                if(canbus_write(&cm)){
                    LOGWARN("Can't write to CANbus, try to reopen");
                    WARNX("Can't write to canbus");
                    if(canbus_disconnected()) reopen_device();
                }
            }
            FREE(mesg);
        }
        usleep(1000);
        if(!canbus_read(&cm)){ // got raw message from CAN bus - parce it
            DBG("Got CAN message from %d, len: %d", cm.ID, cm.len);
            // send raw message to 0
            threadinfo *ti = findThreadByID(0);
            if(ti){
                char buf[64], *ptr = buf;
                int l = 64, x;
                x = snprintf(ptr, l, "#0x%03X ", cm.ID);
                l -= x; ptr += x;
                for(int i = 0; i < cm.len; ++i){
                    x = snprintf(ptr, l, "0x%02X ", cm.data[i]);
                    l -= x; ptr += x;
                }
                addmesg(&ti->answers, buf);
            }
            processCANmessage(&cm);
        }else if(canbus_disconnected()) reopen_device();
    }
    LOGERR("CANserver(): UNREACHABLE CODE REACHED!");
    return NULL;
}

/**
 * @brief stpemulator - stepper motor emulator
 * @param arg - threadinfo
 * @return unused
 */
static void *stpemulator(void *arg){
    threadinfo *ti = (threadinfo*)arg;
    while(1){
        char *mesg = getmesg(&ti->commands);
        if(mesg){
            DBG("Stepper emulator got: %s", mesg);
            addmesg(&ServerMessages, mesg);
            /* do something */
            FREE(mesg);
        }
        int r100 = rand() % 10000;
        if(r100 < 20){ // 20% of probability
            addmesg(&ServerMessages, "stpemulator works fine!");
        }
        if(r100 > 9998){
            addmesg(&ServerMessages, "O that's good!");
        }
        usleep(1000);
    }
    LOGERR("stpemulator(): UNREACHABLE CODE REACHED!");
    return NULL;
}

/**
 * @brief rawcommands - send/receive raw commands
 * @param arg - threadinfo
 * @return unused
 * message format: ID [data]; ID - receiver ID (raw), data - 0..8 bytes of data
 * ID == 0 receive everything!
 */
static void *rawcommands(void *arg){
    threadinfo *ti = (threadinfo*)arg;
    while(1){
        char *mesg = getmesg(&ti->commands);
        if(mesg){
            DBG("Got raw command: %s", mesg);
            addmesg(&CANbusMessages, mesg);
            FREE(mesg);
        }
        mesg = getmesg(&ti->answers);
        if(mesg){ // got raw answer from bus to thread ID, send it to all
            addmesg(&ServerMessages, mesg);
            FREE(mesg);
        }
        usleep(1000);
    }
    LOGERR("rawcommands(): UNREACHABLE CODE REACHED!");
    return NULL;
}

// make string for CAN message from command message (NodeID index subindex [data] -> ID data)
static void sendSDO(char *mesg){
    long info[8] = {0}; // 0 - NodeID, 1 - index, 2 - subindex, 3..6 - data[0..4]
    int N = 0;
    char *saveptr = NULL;
    for(char *s = mesg; N < 8; s = NULL, ++N){
        char *nxt = strtok_r(s, " \t,;\r\n", &saveptr);
        if(!nxt) break;
        if(str2long(nxt, &info[N])) break;
    }
    if(N > 7 || N < 3){
        WARNX("Got bad CANopen command");
        LOGMSG("Got bad CANopen command");
        return;
    }
    DBG("User's message have %d ints", N);

    uint8_t data[8] = {0}, datalen = (uint8_t) N - 3;
    data[0] = SDO_CCS(CCS_INIT_DOWNLOAD);
    if(datalen){ // there's data
        data[0] |= SDO_N(datalen) | SDO_E | SDO_S;
        for(int i = 0; i < datalen; ++i) data[4+i] = (uint8_t)(info[3+i]);
    }
    data[1] = info[1] & 0xff;
    data[2] = (info[1] >> 8) & 0xff;
    data[3] = (uint8_t)(info[2]);

    char buf[64], *ptr = buf;
    int l = 64, x;
    x = snprintf(ptr, l, "0x%03X ", (uint16_t)(RSDO_COBID + info[0]));
    l -= x; ptr += x;
    for(int i = 0; i < 8; ++i){
        x = snprintf(ptr, l, "0x%02X ", (uint16_t)(data[i]));
        l -= x; ptr += x;
    }
    addmesg(&CANbusMessages, buf);
}

// send raw CANopen commands
// message format: NodeID index subindex [data]
static void *canopencmds(void *arg){
    threadinfo *ti = (threadinfo*)arg;
    while(1){
        char *mesg = getmesg(&ti->commands);
        if(mesg) do{
            DBG("Got CANopen command: %s", mesg);
            sendSDO(mesg);
            FREE(mesg);
        }while(0);
        mesg = getmesg(&ti->answers);
        if(mesg){ // got raw answer from bus to thread ID, analize it
addmesg(&ServerMessages, mesg);
            FREE(mesg);
        }
        usleep(1000);
    }
    LOGERR("rawcommands(): UNREACHABLE CODE REACHED!");
    return NULL;
}


/**
 * @brief setCANspeed - set new speed of CANbus
 * @param speed - speed in kbaud
 * @return 0 if all OK
 */
int setCANspeed(int speed){
    if(canbus_setspeed(speed)) return 1;
    CANspeed = speed;
    return 0;
}

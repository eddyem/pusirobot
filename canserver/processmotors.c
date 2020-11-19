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
#include "canopen.h"
#include "cmdlnopts.h"
#include "processmotors.h"
#include "pusirobot.h"
#include "socket.h"

#include <fcntl.h>      // open
#include <inttypes.h>   // PRId64
#include <stdio.h>      // printf
#include <string.h>     // strcmp
#include <sys/stat.h>   // open
#include <unistd.h>     // usleep
#include <usefull_macros.h>

static int CANspeed = 0; // default speed, if !=0 set it when connected

// all messages are in format "ID [data]"
static message CANbusMessages = {0}; // CANserver thread is master
#define CANBUSPUSH(mesg)    mesgAddObj(&CANbusMessages, mesg, sizeof(CANmesg))
#define CANBUSPOP()         mesgGetObj(&CANbusMessages, NULL)

// commands sent to threads
// each threadCmd array should be terminated with NULLs; default command `help` shows all names/descriptions
typedef struct{
    char *name;     // command name
    int nargs;      // max arguments number
    long *args;     // pointer to arguments
    char *descr;    // description for help
} threadCmd;

// basic threads
// messages: master - thread, slave - caller
static void *stpemulator(void *arg);
static void *rawcommands(void *arg);
static void *canopencmds(void *arg);
static void *simplestp(void *arg);

// handlers for standard types
thread_handler CANhandlers[] = {
    {"canopen", canopencmds, "NodeID index subindex [data] - raw CANOpen commands with `index` and `subindex` to `NodeID`"},
    {"emulation", stpemulator, "(list) - stepper emulation"},
    {"raw", rawcommands, "ID [DATA] - raw CANbus commands to raw `ID` with `DATA`"},
    {"stepper", simplestp, "(list) - simple stepper motor: no limit switches, only goto"},
    {NULL, NULL, NULL}
};

thread_handler *get_handler(const char *name){
    for(thread_handler *ret = CANhandlers; ret->name; ++ret){
        if(strcmp(ret->name, name)) continue;
        return ret;
    }
    return NULL;
}

/**
 * @brief cmdParser - parser of user's comands
 * @param cmdlist - NULL-terminated array with possible commands
 * @param cmd - user's command (default command `list` shows all names/descriptions)
 * @return index of command in `cmdlist` or -1 if something wrong or not found
 */
static int cmdParser(threadCmd *cmdlist, char *s){
    if(!cmdlist || !s) return -1;
    char *saveptr, *command;
    int idx = 0;
    command = strtok_r(s, " \t,;\r\n", &saveptr);
    if(!command) return -1;
    if(strcmp(command, "list") == 0){ // just show help
        char buf[128];
        while(cmdlist->name){
            snprintf(buf, 128, "params> %s (%d arguments) - %s",
                     cmdlist->name, cmdlist->nargs, cmdlist->descr);
            mesgAddText(&ServerMessages, buf);
            ++cmdlist;
        }
        return 10000;
    }
    while(cmdlist->name){
        if(strcmp(cmdlist->name, command) == 0) break;
        ++idx; ++cmdlist;
    }
    if(!cmdlist->name) return -1; // command not found
    if(cmdlist->nargs == 0) return idx; // simple command
    int nargsplus1 = cmdlist->nargs + 1, N = 0;
    long *args = MALLOC(long, nargsplus1);
    for(; N < nargsplus1; ++N){
        char *nxt = strtok_r(NULL, " \t,;\r\n", &saveptr);
        if(!nxt) break;
        if(str2long(nxt, &args[N])) break;
    }
    if(N != cmdlist->nargs){
        FREE(args);
        return -1; // bad arguments number
    }
    for(int i = 0; i < N; ++i)
        cmdlist->args[i] = args[i];
    FREE(args);
    return idx;
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
    threadinfo *ti = findThreadByID(0);
    if(ti){
        mesgAddObj(&ti->answers, (void*)mesg, sizeof(CANmesg));
    }
    ti = findThreadByID(mesg->ID);
    if(ti){
        mesgAddObj(&ti->answers, (void*) mesg, sizeof(CANmesg));
    }
}

/**
 * @brief CANserver - main CAN thread; receive/transmit raw messages by CANbusMessages
 * @param data - unused
 * @return unused
 */
void *CANserver(_U_ void *data){
    reopen_device();
    while(1){
        CANmesg *msg = CANBUSPOP();
        if(msg){
            if(canbus_write(msg)){
                LOGWARN("Can't write to CANbus, try to reopen");
                WARNX("Can't write to canbus");
                if(canbus_disconnected()) reopen_device();
            }
            FREE(msg);
        }
        usleep(1000);
        CANmesg cm = {0};
        if(!canbus_read(&cm)){ // got raw message from CAN bus - parce it
            DBG("Got CAN message from %d, len: %d", cm.ID, cm.len);
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
        char *mesg = mesgGetText(&ti->commands);
        if(mesg){
            DBG("Stepper emulator got: %s", mesg);
            mesgAddText(&ServerMessages, mesg);
            /* do something */
            FREE(mesg);
        }
        int r100 = rand() % 10000;
        if(r100 < 1){ // 10% of probability
            mesgAddText(&ServerMessages, "stpemulator works fine!");
        }
        if(r100 > 9998){
            mesgAddText(&ServerMessages, "O that's good!");
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
        char *mesg = mesgGetText(&ti->commands);
        if(mesg){
            DBG("Got raw command: %s", mesg);
            CANmesg cm;
            if(!parsePacket(&cm, mesg)) CANBUSPUSH(&cm);
            FREE(mesg);
        }
        CANmesg *ans = (CANmesg*) mesgGetObj(&ti->answers, NULL);
        if(ans){ // got raw answer from bus to thread ID, send it to all
            char buf[64], *ptr = buf;
            int l = 64, x;
            x = snprintf(ptr, l, "#0x%03X ", ans->ID);
            l -= x; ptr += x;
            for(int i = 0; i < ans->len; ++i){
                x = snprintf(ptr, l, "0x%02X ", ans->data[i]);
                l -= x; ptr += x;
            }
            mesgAddText(&ServerMessages, buf);
            FREE(ans);
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

    CANmesg comesg;
    uint8_t datalen = (uint8_t) N - 3;
    comesg.data[0] = (datalen) ? SDO_CCS(CCS_INIT_DOWNLOAD) : SDO_CCS(CCS_INIT_UPLOAD); // write or read
    comesg.len = 8;
    if(datalen){ // there's data
        comesg.data[0] |= SDO_N(datalen) | SDO_E | SDO_S;
        for(int i = 0; i < datalen; ++i) comesg.data[4+i] = (uint8_t)(info[3+i]);
    }
    comesg.data[1] = info[1] & 0xff;
    comesg.data[2] = (info[1] >> 8) & 0xff;
    comesg.data[3] = (uint8_t)(info[2]);
    comesg.ID = (uint16_t)(RSDO_COBID + info[0]);
    CANBUSPUSH(&comesg);
}

// send raw CANopen commands
// message format: NodeID index subindex [data]
static void *canopencmds(void *arg){
    threadinfo *ti = (threadinfo*)arg;
    while(1){
        char *mesg = mesgGetText(&ti->commands);
        if(mesg) do{
            DBG("Got CANopen command: %s", mesg);
            sendSDO(mesg);
            FREE(mesg);
        }while(0);
        CANmesg *ans = (CANmesg*)mesgGetObj(&ti->answers, NULL);
        if(ans){ // got raw answer from bus to thread ID, analize it
            SDO sdo;
            if(parseSDO(ans, &sdo)){
                char buf[128], *ptr = buf;
                int rest = 128;
                int l = snprintf(ptr, rest, "%s nid=0x%02X, idx=0x%04X, subidx=%d, ccs=0x%02X, datalen=%d",
                         ti->name, sdo.NID, sdo.index, sdo.subindex, sdo.ccs, sdo.datalen);
                ptr += l; rest -= l;
                if(sdo.datalen){
                    l = snprintf(ptr, rest, ", data=[");
                    ptr += l; rest -= l;
                    for(int idx = 0; idx < sdo.datalen; ++idx){
                        if(idx) l = snprintf(ptr, rest, ", 0x%02X", sdo.data[idx]);
                        else l = snprintf(ptr, rest, "0x%02X", sdo.data[idx]);
                        ptr += l; rest -= l;
                    }
                    l = snprintf(ptr, rest, "]");
                    ptr += l; rest -= l;
                }
                mesgAddText(&ServerMessages, buf);
            }
            FREE(ans);
        }
        usleep(1000);
    }
    LOGERR("rawcommands(): UNREACHABLE CODE REACHED!");
    return NULL;
}

// check incoming SDO and send data to all (thrname - thread name)
static void chkSDO(const SDO *sdo, const char *thrname){
    char buf[128];
    if(!sdo) return;
    SDO_dic_entry *de = dictentry_search(sdo->index, sdo->subindex);
    if(!de) return; // SDO not from dictionary
    const abortcodes *ac = NULL;
    int64_t val = getSDOval(sdo, de, &ac);
    if(val == INT_MAX) // zero-length SDO - last command acknowledgement
        snprintf(buf, 128, "%s %s=OK", thrname, de->varname);
    else if(val == INT64_MIN) // error
        snprintf(buf, 128, "%s abortcode='0x%X' error='%s'", thrname, ac->code, ac->errmsg);
    else // got value
        snprintf(buf, 128, "%s %s=%" PRId64, thrname, de->varname, val);
    mesgAddText(&ServerMessages, buf);
}

/**
 * @brief simplestp - simplest stepper motor
 * @param arg - thread identifier
 * @return not used
 * Commands:
 *      maxspeed x: set maximal speed to x pulses per second
 *      microsteps x: set microstepping to x pulses per step
 *      move x: move for x pulses (+-)
 *      status: current position & state
 *      stop: stop motor
 */
static void *simplestp(void *arg){
    threadinfo _U_ *ti = (threadinfo*)arg;
    CANmesg can;
    char buf[128];
    long parameters[2];
    threadCmd commands[] = {
        [0] = {"test", 2, parameters, "show test values"},
        [1] = {"status", 0, NULL, "get current position and status"},
        [2] = {"stop", 0, NULL, "stop motor"},
        [3] = {"absmove", 1, parameters, "absolute move to position x"},
        {NULL, 0, NULL, NULL}
    };
    int NID = ti->ID & NODEID_MASK; // node ID
    // prepare all
    CANBUSPUSH(SDO_write(&MAXSPEED, NID, 3200, &can));
    while(1){
        char *mesg = mesgGetText(&ti->commands);
        if(mesg) do{
            DBG("Got command: %s", mesg);
            int idx = cmdParser(commands, mesg);
            DBG("idx = %d", idx);
            if(-1 == idx){
                snprintf(buf, 128, "%s wrong command '%s' or bad arguments number", ti->name, mesg);
                mesgAddText(&ServerMessages, buf);
            }
            switch(idx){
                case 0:
                    snprintf(buf, 128, "%s get test params: %ld, %ld", ti->name, parameters[0], parameters[1]);
                    mesgAddText(&ServerMessages, buf);
                break;
                case 1: // status, curpos
                    CANBUSPUSH(SDO_read(&DEVSTATUS, NID, &can));
                    CANBUSPUSH(SDO_read(&POSITION, NID, &can));
                    CANBUSPUSH(SDO_read(&ERRSTATE, NID, &can));
                break;
                case 2: // stop
                    CANBUSPUSH(SDO_write(&STOP, NID, 1, &can));
                break;
                case 3: // absmove
                    /*if(parameters[0] < 0){
                        CANBUSPUSH(SDO_write(&RELSTEPS, NID, parameters[0], &can));
                        parameters[0] = -parameters[0];
                    }*/
                    CANBUSPUSH(SDO_write(&ABSSTEPS, NID, parameters[0], &can));
                break;
                default:
                break;
            }
            FREE(mesg);
        }while(0);
        CANmesg *ans = (CANmesg*)mesgGetObj(&ti->answers, NULL);
        if(ans) do{
            SDO sdo;
            if(!parseSDO(ans, &sdo)) break;
            chkSDO(&sdo, ti->name);
            FREE(ans);
        }while(0);
        usleep(1000);
    }
    LOGERR("simplestp(): UNREACHABLE CODE REACHED!");
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

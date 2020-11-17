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
#include "cmdlnopts.h"
#include "processmotors.h"
#include "proto.h"
#include "socket.h"
#include "threadlist.h"

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

// standard answers of processCommand
static const char *ANS_OK = "OK";
static const char *ANS_WRONGCANID = "Wrong CANID";
static const char *ANS_NOTFOUND = "Thread not found";
static const char *ANS_CANTSEND = "Can't send message";

static const char *listthr(_U_ char *par1, _U_ char *par2);
static const char *regthr(char *thrname, char *data);
static const char *unregthr(char *thrname, char *data);
static const char *sendmsg(char *thrname, char *data);
static const char *setspd(char *speed, _U_ char *data);

/*
 * Commands format:
 * function [[NAME] data]
 *      where:
 *      - function - function name (register, help, unregister, stop, absmove etc),
 *      - NAME - thread name,
 *      - data - function data (e.g. relmove turret1 150)
 * you can get full list of functions by function `help`
 */
typedef struct{
    const char *fname; // function name
    const char *(*handler)(char *arg1, char *arg2); // data handler (arg1 and arg2 could be changed)
} cmditem;

// array with known functions
static cmditem functions[] = {
    {"list", listthr},          // list threads
    {"mesg", sendmsg},          // "mesg NAME ID [data]"
    {"register", regthr},       // "register NAME ID", ID - RAW CAN ID (not canopen ID)!!!
    {"speed", setspd},          // set CANbus speed
    {"unregister", unregthr},   // "unregister NAME"
    {NULL, NULL}
};

// list all threads
static const char *listthr(_U_ char *par1, _U_ char *par2){
    FNAME();
    char msg[256];
    threadlist *list = NULL;
    int empty = 1;
    do{
        list = nextThread(list);
        if(!list) break;
        snprintf(msg, 256, "thread name='%s' role='%s' ID=0x%X", list->ti.name, list->ti.handler.name, list->ti.ID);
        addmesg(&ServerMessages, msg);
        empty = 0;
    }while(1);
    if(empty) return "No threads";
    return NULL;
}

// register new thread
/**
 * @brief regthr  - register new thread
 * @param thrname - thread name
 * @param data - CANID and thread role
 * @return answer to client
 */
static const char *regthr(char *thrname, char *data){
    FNAME();
    threadinfo *ti = findThreadByName(thrname);
    if(ti) return "Thread exists";
    char *saveptr;
    char *id = strtok_r(data, " \t,;\r\n", &saveptr);
    if(!id) return ANS_WRONGCANID;
    char *role = strtok_r(NULL, " \t,;\r\n", &saveptr);
    if(!role) return "No thread role";
    DBG("Data='%s'; id='%s', role='%s'", data, id, role);
    long ID;
    if(str2long(data, &ID)){
        return ANS_WRONGCANID;
    }
    DBG("Check ID");
    ti = findThreadByID(ID);
    if(ti) return "Thread with given ID exists";
    thread_handler *h = get_handler(role);
    if(!h) return "Unknown role";
    if(!registerThread(thrname, ID, h)) return "Can't register";
    return ANS_OK;
}

/**
 * @brief unregthr - delete thread
 * @param thrname - thread's name
 * @param data - unused
 * @return answer
 */
static const char *unregthr(char *thrname, _U_ char *data){
    FNAME();
    if(killThreadByName(thrname)) return ANS_NOTFOUND;
    return ANS_OK;
}

/**
 * @brief sendmsg - send message to given thread
 * @param thrname - thread's naem
 * @param data - data to send
 * @return answer
 */
static const char *sendmsg(char *thrname, char *data){
    FNAME();
    threadinfo *ti = findThreadByName(thrname);
    if(!ti) return ANS_NOTFOUND;
    if(!addmesg(&ti->commands, data)) return ANS_CANTSEND;
    return ANS_OK;
}

static const char *setspd(char *speed, _U_ char *data){
    FNAME();
    long spd;
    if(str2long(speed, &spd) || spd < 1 || spd > 1000 || setCANspeed((int)spd)){
        DBG("Wrong speed: %s", speed);
        return "Wrong speed";
    }
    return ANS_OK;
}

/**
 * @brief processCommand - parse command received by socket
 * @param cmd (io) - text command (after this function its content will be broken!)
 * @return NULL or error answer to user
 */
const char *processCommand(char *cmd){
    if(!cmd) return NULL;
    char *saveptr = NULL, *fname = NULL, *procname = NULL, *data = NULL;
    DBG("Got %s", cmd);
    fname = strtok_r(cmd, " \t\r\n", &saveptr);
    DBG("fname: %s", fname);
    if(fname){
        procname = strtok_r(NULL, " \t\r\n", &saveptr);
        DBG("procname: %s", procname);
        if(procname){
            data = saveptr;
            DBG("data: %s", data);
        }
    }else return NULL;
    for(cmditem *item = functions; item->fname; ++item){
        if(0 == strcasecmp(item->fname, fname)) return item->handler(procname, data);
    }
    return "Wrong command";
}


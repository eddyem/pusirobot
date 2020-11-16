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
#include "threadlist.h"

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

// standard answers of processCommand
static const char *ANS_OK = "OK\n";
static const char *ANS_WRONGCANID = "Wrong CANID\n";
static const char *ANS_NOTFOUND = "Thread not found\n";
static const char *ANS_CANTSEND = "Can't send message\n";

static const char *sendraw(char *id, char *data);
static const char *regthr(char *thrname, char *data);
static const char *unregthr(char *thrname, char *data);
static const char *sendmsg(char *thrname, char *data);

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
    {"raw", sendraw},
    {"register", regthr},
    {"unregister", unregthr},
    {"mesg", sendmsg},
    {NULL, NULL}
};

/**
 * @brief sendraw - send raw data to CANbus
 * @param id    - CANid (in string format)
 * @param data  - data to send (delimeters are: space, tab, comma or semicolon)
 *          WARNING! parameter `data` will be broken after this function
 *              id & data can be decimal, hexadecimal or octal
 * @return answer to client
 */
static const char *sendraw(char *id, char *data){
    char buf[128], *s, *saveptr;
    if(!id) return "Need CAN ID\n";
    long ID, info[9]={0};
    int i;
    if(str2long(id, &ID)){
        return ANS_WRONGCANID;
    }
    for(s = data, i = 0; i < 9; s = NULL, ++i){
        char *nxt = strtok_r(s, " \t,;\r\n", &saveptr);
        if(!nxt) break;
        if(str2long(nxt, &info[i])) break;
    }
    if(i > 8) return "Not more than 8 data bytes\n";
    snprintf(buf, 128, "ID=%ld, datalen=%d, data={%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld}\n",
             ID, i, info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
    addmesg(idxMISO, &CANbusMessages, buf);
    return ANS_OK;
}

// register new thread
/**
 * @brief regthr  - register new thread
 * @param thrname - thread name
 * @param data - CANID and thread role
 * @return answer to client
 */
static const char *regthr(char *thrname, char *data){
    threadinfo *ti = findThreadByName(thrname);
    if(ti) return "Thread exists\n";
    char *saveptr;
    char *id = strtok_r(data, " \t,;\r\n", &saveptr);
    if(!id) return ANS_WRONGCANID;
    char *role = strtok_r(NULL, " \t,;\r\n", &saveptr);
    if(!role) return "No thread role\n";
    DBG("Data='%s'; id='%s', role='%s'", data, id, role);
    long ID;
    if(str2long(data, &ID)){
        return ANS_WRONGCANID;
    }
    DBG("Check ID");
    ti = findThreadByID(ID);
    if(ti) return "Thread with given ID exists\n";
    thread_handler *h = get_handler(role);
    if(!h) return "Unknown role\n";
    if(!registerThread(thrname, ID, h->handler)) return "Can't register\n";
    return ANS_OK;
}

/**
 * @brief unregthr - delete thread
 * @param thrname - thread's name
 * @param data - unused
 * @return answer
 */
static const char *unregthr(char *thrname, _U_ char *data){
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
    threadinfo *ti = findThreadByName(thrname);
    if(!ti) return ANS_NOTFOUND;
    if(!addmesg(idxMISO, &ti->mesg, data)) return ANS_CANTSEND;
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
    return "Wrong command\n";
}

#if 0
static char buf[1024];
snprintf(buf, 1024, "FUNC=%s, PROC=%s, CMD=%s\n", fname, procname, data);
DBG("buf: %s", buf);
return buf;
#endif

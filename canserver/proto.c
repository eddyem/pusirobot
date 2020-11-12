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
#include "proto.h"

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

/**
 * @brief sendraw - send raw data to CANbus
 * @param id    - CANid (in string format)
 * @param data  - data to send (delimeters are: space, tab, comma or semicolon)
 *          WARNING! parameter `data` will be broken after this function
 *              id & data can be decimal, hexadecimal or octal
 * @return answer to client
 */
char *sendraw(char *id, char *data){
    char buf[128], *s, *saveptr;
    if(!id) return strdup("Need CAN ID\n");
    long ID, info[8]={0};
    int i;
    if(str2long(id, &ID)){
        snprintf(buf, 128, "Wrong ID: %s\n", id);
        return strdup(buf);
    }
    for(s = data, i = 0; ; s = NULL, ++i){
        char *nxt = strtok_r(s, " \t,;\r\n", &saveptr);
        if(!nxt) break;
        if(str2long(nxt, &info[i])) break;
    }
    if(i > 8) return strdup("Not more than 8 data bytes\n");
    snprintf(buf, 128, "ID=%ld, datalen=%d, data={%ld, %ld, %ld, %ld, %ld, %ld, %ld, %ld}\n",
             ID, i, info[0], info[1], info[2], info[3], info[4], info[5], info[6], info[7]);
    return strdup(buf);
}

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
    char *fname;
    char *(*handler)(char *arg1, char *arg2);
} cmditem;

// array with known functions
static cmditem functions[] = {
    {"raw", sendraw},
    {NULL, NULL}
};

/**
 * @brief processCommand - parse command received by socket
 * @param cmd (io) - text command (after this function its content will be broken!)
 * @return answer to user (or NULL if none)  !!!ALLOCATED HERE, should be FREEd!!!
 */
char *processCommand(char *cmd){
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
    return strdup("Wrong command\n");
}

#if 0
static char buf[1024];
snprintf(buf, 1024, "FUNC=%s, PROC=%s, CMD=%s\n", fname, procname, data);
DBG("buf: %s", buf);
return buf;
#endif

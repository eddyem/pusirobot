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

#pragma once
#ifndef __CMDLNOPTS_H__
#define __CMDLNOPTS_H__

#include <pthread.h>
#include <usefull_macros.h>
#include "term.h"

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *pidfile;          // name of PID file (default: /tmp/canserver.pid)
    char *device;           // serial device name
    char *vid;              // vendor id
    char *pid;              // product id
    char *port;             // port to connect
    char *logfile;          // logfile name
    int verb;               // increase logfile verbosity level
    int terminal;           // run as terminal
    int echo;               // echo user commands back
    int rest_pars_num;      // number of rest parameters
    char** rest_pars;       // the rest parameters: array of char* (path to logfile and thrash)
} glob_pars;


glob_pars *parse_args(int argc, char **argv);

extern glob_pars *GP;

#endif // __CMDLNOPTS_H__

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
#ifndef AUX_H__
#define AUX_H__

#include "cmdlnopts.h"

typedef enum{
    LOGLEVEL_NONE,  // no logs
    LOGLEVEL_ERR,   // only errors
    LOGLEVEL_WARN,  // only warnings and errors
    LOGLEVEL_MSG,   // all without debug
    LOGLEVEL_DBG,   // all messages
    LOGLEVEL_ANY    // all shit
} Cl_loglevel;

typedef struct{
    char *logpath;          // full path to logfile
    Cl_loglevel loglevel;   // loglevel
    pthread_mutex_t mutex;  // log mutex
} Cl_log;

extern Cl_log *globlog; // global log file

Cl_log *Cl_createlog(const char *logpath, Cl_loglevel level);
#define OPENLOG(nm, lvl)   (globlog = Cl_createlog(nm, lvl))
void Cl_deletelog(Cl_log **log);
int Cl_putlogt(int timest, Cl_log *log, Cl_loglevel lvl, const char *fmt, ...);
// shortcuts for different log levels; ..ADD - add message without timestamp
#define LOGERR(...)     do{Cl_putlogt(1, globlog, LOGLEVEL_ERR, __VA_ARGS__);}while(0)
#define LOGERRADD(...)  do{Cl_putlogt(1, globlog, LOGLEVEL_ERR, __VA_ARGS__);}while(0)
#define LOGWARN(...)    do{Cl_putlogt(1, globlog, LOGLEVEL_WARN, __VA_ARGS__);}while(0)
#define LOGWARNADD(...) do{Cl_putlogt(0, globlog, LOGLEVEL_WARN, __VA_ARGS__);}while(0)
#define LOGMSG(...)     do{Cl_putlogt(1, globlog, LOGLEVEL_MSG, __VA_ARGS__);}while(0)
#define LOGMSGADD(...)  do{Cl_putlogt(0, globlog, LOGLEVEL_MSG, __VA_ARGS__);}while(0)
#define LOGDBG(...)     do{Cl_putlogt(1, globlog, LOGLEVEL_DBG, __VA_ARGS__);}while(0)
#define LOGDBGADD(...)  do{Cl_putlogt(0, globlog, LOGLEVEL_DBG, __VA_ARGS__);}while(0)

char *find_device();

int str2long(char *str, long* l);

#endif // AUX_H__

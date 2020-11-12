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
#ifndef THREADLIST_H__
#define THREADLIST_H__

#include <pthread.h>

// max length (in symbols) of thread name (any zero-terminated string)
#define THREADNAMEMAXLEN    (31)

// messages FIFO
typedef struct msglist_{
    char *data;                     // message itself
    struct msglist_ *next, *last;   // other elements of list
} msglist;

typedef enum{
    idxMOSI = 0,    // master out, slave in
    idxMISO = 1,    // master in, slave out
    idxNUM  = 2     // amount of indexes
} msgidx;

// interthread messages; index 0 - MOSI, index 1 - MISO
typedef struct{
    msglist *text[idxNUM];          // stringified text messages
    pthread_mutex_t mutex[idxNUM];  // text changing mutex
} message;

// thread information
typedef struct{
    char name[THREADNAMEMAXLEN+1];  // thread name
    int ID;                         // numeric ID (canopen ID)
    message mesg;                   // inter-thread messages
    pthread_t thread;               // thread descriptor
    void *(*handler)(void *);       // handler function
} threadinfo;

// list of threads member
typedef struct thread_list_{
    threadinfo ti;                  // base thread information
    struct thread_list_ *next;      // next element
} threadlist;

threadinfo *findThreadByName(char *name);
threadinfo *findThreadByID(int ID);
threadinfo *registerThread(char *name, int ID, void *(*handler)(void *));
int killThread(const char *name);
char *getmesg(msgidx idx, message *msg);
char *addmesg(msgidx idx, message *msg, char *txt);

#endif // THREADLIST_H__

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
    void *data;                     // message itself
    size_t size;                    // message length in bytes
    struct msglist_ *next, *last;   // other elements of list
} msglist;

// interthread messages; index 0 - MOSI, index 1 - MISO
typedef struct{
    msglist *msg;          // stringified text messages
    pthread_mutex_t mutex;  // text changing mutex
} message;

// name - handler pair for threads registering functions
typedef struct{
    const char *name;               // handler name
    void *(*handler)(void *);       // handler function
    const char *helpmesg;           // help message
} thread_handler;

// thread information
typedef struct{
    char name[THREADNAMEMAXLEN+1];  // thread name
    int ID;                         // numeric ID (canopen ID)
    message commands;               // commands from clients (char *)
    message answers;                // answers from CANserver (CANmesg *)
    pthread_t thread;               // thread descriptor
    thread_handler handler;         // handler name & function
} threadinfo;

// list of threads member
typedef struct thread_list_{
    threadinfo ti;                  // base thread information
    struct thread_list_ *next;      // next element
} threadlist;

threadinfo *findThreadByName(char *name);
threadinfo *findThreadByID(int ID);
threadinfo *registerThread(char *name, int ID, thread_handler *handler);
threadlist *nextThread(threadlist *curr);
int killThreadByName(const char *name);
char *mesgGetText(message *msg);
char *mesgAddText(message *msg, char *txt);
void *mesgGetObj(message *msg, size_t *size);
void *mesgAddObj(message *msg, void *data, size_t size);

#endif // THREADLIST_H__

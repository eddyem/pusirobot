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

#include "threadlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usefull_macros.h>

// global thread list
static threadlist *thelist = NULL;

/**
 * push data into the tail of a list (FIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pushed node
 */
static msglist *pushmessage(msglist **lst, void *v, size_t size){
    if(!lst || !v) return NULL;
    msglist *node;
    if((node = MALLOC(msglist, 1)) == 0)
        return NULL; // allocation error
    node->data = malloc(size);
    if(!node->data){
        FREE(node);
        return NULL;
    }
    memcpy(node->data, v, size);
    node->size = size;
    if(!*lst){
        *lst = node;
        (*lst)->last = node;
        return node;
    }
    (*lst)->last->next = node;
    (*lst)->last = node;
    return node;
}

/**
 * @brief popmessage - get data from head of list
 * @param lst (io) - list
 * @param size (o) - data size
 * @return data from first node or NULL if absent  (SHOULD BE FREEd AFER USAGE!)
 */
static void *popmessage(msglist **lst, size_t *size){
    if(!lst || !*lst) return NULL;
    char *ret;
    msglist *node = *lst;
    if(node->next) node->next->last = node->last; // pop not last message
    ret = node->data;
    if(size) *size = node->size;
    *lst = node->next;
    FREE(node);
    return ret;
}

/**
 * @brief getlast - find last element in thread list
 * @return pointer to the last element or NULL if list is empty
 */
static threadlist *getlast(){
    if(!thelist) return NULL;
    threadlist *t = thelist;
    while(t->next) t = t->next;
    return t;
}

/**
 * @brief findThreadByName - find thread by name
 * @param name - thread name
 * @return thread found or NULL if absent
 */
threadinfo *findThreadByName(char *name){
    if(!name) return NULL;
    if(!thelist) return NULL; // thread list is empty
    size_t l = strlen(name);
    if(l < 1 || l > THREADNAMEMAXLEN) return NULL;
    DBG("Try to find thread '%s'", name);
    threadlist *lptr = thelist;
    while(lptr){
        DBG("Check '%s'", lptr->ti.name);
        if(strcmp(lptr->ti.name, name) == 0) return &lptr->ti;
        lptr = lptr->next;
    }
    return NULL;
}

/**
 * @brief findThreadByID - find thread by numeric ID
 * @param ID - thread ID
 * @return thread found or NULL if absent
 */
threadinfo *findThreadByID(int ID){
    if(!thelist) return NULL; // thread list is empty
    threadlist *lptr = thelist;
    while(lptr){
        if(ID == lptr->ti.ID) return &lptr->ti;
        lptr = lptr->next;
    }
    return NULL;
}

/**
 * @brief mesgAddObj - add any object to message
 * @param msg  (i) - message
 * @param data (i) - any data
 * @param size     - it's size
 * @return pointer to message data if success (NULL if failed)
 */
void *mesgAddObj(message *msg, void *data, size_t size){
    if(!msg || !data || size == 0) return NULL;
    DBG("Want to add mesg with length %zd", size);
    if(pthread_mutex_lock(&msg->mutex)) return NULL;
    msglist *node = pushmessage(&msg->msg, data, size);
    if(!node){
        pthread_mutex_unlock(&msg->mutex);
        return NULL;
    }
    pthread_mutex_unlock(&msg->mutex);
    return node->data;
}

/**
 * @brief mesgAddText - add message to thread's queue
 * @param msg - message itself
 * @param txt - data to add
 * @return data added or NULL if failed
 */
char *mesgAddText(message *msg, char *txt){
    if(!txt) return NULL;
    DBG("mesgAddText(%s)", txt);
    size_t l = strlen(txt) + 1;
    return mesgAddObj(msg, (void*)txt, l);
}

/**
 * @brief mesgGetObj - get object from message
 * @param msg (i)  - message
 * @param size (o) - object's size or NULL
 * @return pointer to object or NULL if absent
 */
void *mesgGetObj(message *msg, size_t *size){
    if(!msg) return NULL;
    char *text = NULL;
    if(pthread_mutex_lock(&msg->mutex)) return NULL;
    text = popmessage(&msg->msg, size);
    pthread_mutex_unlock(&msg->mutex);
    return text;
}

/**
 * @brief mesgGetText - get first message from queue (allocates data, should be free'd after usage!)
 * @param msg - message itself
 * @return data or NULL if empty
 */
char *mesgGetText(message *msg){
    return (char*) mesgGetObj(msg, NULL);
}

/**
 * @brief registerThread - register new thread
 * @param name    - thread name
 * @param ID      - thread numeric ID
 * @param handler - thread handler
 * @return pointer to new threadinfo struct or NULL if failed
 */
threadinfo *registerThread(char *name, int ID, thread_handler *handler){
    if(!name || strlen(name) < 1 || !handler) return NULL;
    threadinfo *ti = findThreadByName(name);
    DBG("Register new thread with name '%s' and ID=%d", name, ID);
    if(ti){
        WARNX("Thread named '%s' exists!", name);
        return NULL;
    }
    ti = findThreadByID(ID);
    if(ti){
        WARNX("Thread with ID=%d exists!", ID);
        return NULL;
    }
    if(!thelist){ // the first element
        thelist = MALLOC(threadlist, 1);
        ti = &thelist->ti;
    }else{
        threadlist *last = getlast();
        last->next = MALLOC(threadlist, 1);
        ti = &last->next->ti;
    }
    memcpy(&ti->handler, handler, sizeof(thread_handler));
    snprintf(ti->name, THREADNAMEMAXLEN+1, "%s", name);
    ti->ID = ID;
    memset(&ti->commands, 0, sizeof(ti->commands));
    pthread_mutex_init(&ti->commands.mutex, NULL);
    memset(&ti->answers, 0, sizeof(ti->answers));
    pthread_mutex_init(&ti->answers.mutex, NULL);
    if(pthread_create(&ti->thread, NULL, handler->handler, (void*)ti)){
        WARN("pthread_create()");
        return NULL;
    }
    return ti;
}

/**
 * @brief killThread - kill thread by its descriptor
 * @param lptr - pointer to thread descriptor
 * @param prev - pointer to previous thread in list or NULL (to found it here)
 * @return 0 if all OK
 */
int killThread(threadlist *lptr, threadlist *prev){
    if(!lptr) return 1;
    if(!prev){
        threadlist *t = thelist;
        for(; t; t = t->next){
            if(t == lptr) break;
            prev = lptr;
        }
    }
    DBG("Delete '%s', prev: '%s'", lptr->ti.name, prev->ti.name);
    threadlist *next = lptr->next;
    if(lptr == thelist) thelist = next;
    else if(prev) prev->next = next;
    char *txt;
    pthread_mutex_lock(&lptr->ti.commands.mutex);
    while((txt = popmessage(&lptr->ti.commands.msg, NULL))) FREE(txt);
    pthread_mutex_destroy(&lptr->ti.commands.mutex);
    pthread_mutex_lock(&lptr->ti.answers.mutex);
    while((txt = popmessage(&lptr->ti.answers.msg, NULL))) FREE(txt);
    pthread_mutex_destroy(&lptr->ti.answers.mutex);
    if(pthread_cancel(lptr->ti.thread)) WARN("Can't kill thread '%s'", lptr->ti.name);
    FREE(lptr);
    return 0;
}

/**
 * @brief killThread - kill and unregister thread with given name
 * @param name - thread's name
 * @return 0 if all OK
 */
int killThreadByName(const char *name){
    if(!name || !thelist) return 1;
    threadlist *lptr = thelist, *prev = NULL;
    for(; lptr; lptr = lptr->next){
        if(strcmp(lptr->ti.name, name)){
            prev = lptr;
            continue;
        }
        return killThread(lptr, prev);
    }
    return 2; // not found
}

/**
 * @brief nextThread - get next thread in `thelist`
 * @param curr - pointer to previous thread or NULL for `thelist`
 * @return pointer to next thread in list (or NULL if absent)
 */
threadlist *nextThread(threadlist *curr){
    if(!curr) return thelist;
    return curr->next;
}



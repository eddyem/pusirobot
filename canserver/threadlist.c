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
static msglist *pushmessage(msglist **lst, char *v){
    if(!lst || !v) return NULL;
    msglist *node;
    if((node = MALLOC(msglist, 1)) == 0)
        return NULL; // allocation error
    node->data = strdup(v);
    if(!node->data){
        FREE(node);
        return NULL;
    }
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
 * @return data from first node or NULL if absent  (SHOULD BE FREEd AFER USAGE!)
 */
static char *popmessage(msglist **lst){
    if(!lst || !*lst) return NULL;
    char *ret;
    msglist *node = *lst;
    if(node->next) node->next->last = node->last; // pop not last message
    ret = node->data;
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
    DBG("Try to find thread with ID=%d", ID);
    threadlist *lptr = thelist;
    while(lptr){
        DBG("Check %d", lptr->ti.ID);
        if(ID == lptr->ti.ID) return &lptr->ti;
        lptr = lptr->next;
    }
    return NULL;
}

/**
 * @brief addmesg - add message to thread's queue
 * @param idx - index (MOSI/MISO)
 * @param msg - message itself
 * @param txt - data to add
 * @return data added or NULL if failed
 */
char *addmesg(msgidx idx, message *msg, char *txt){
    if(idx < 0 || idx >= idxNUM){
        WARNX("Wrong message index");
        return NULL;
    }
    if(!msg) return NULL;
    size_t L = strlen(txt);
    if(L < 1) return NULL;
    DBG("Want to add mesg '%s' with length %zd", txt, L);
    if(pthread_mutex_lock(&msg->mutex[idx])) return NULL;
    msglist *node = pushmessage(&msg->text[idx], txt);
    if(!node){
        pthread_mutex_unlock(&msg->mutex[idx]);
        return NULL;
    }
    pthread_mutex_unlock(&msg->mutex[idx]);
    return node->data;
}

/**
 * @brief getmesg - get first message from queue (allocates data, should be free'd after usage!)
 * @param idx - index (MOSI/MISO)
 * @param msg - message itself
 * @return data or NULL if empty
 */
char *getmesg(msgidx idx, message *msg){
    if(idx < 0 || idx >= idxNUM){
        WARNX("Wrong message index");
        return NULL;
    }
    if(!msg) return NULL;
    char *text = NULL;
    if(pthread_mutex_lock(&msg->mutex[idx])) return NULL;
    text = popmessage(&msg->text[idx]);
    pthread_mutex_unlock(&msg->mutex[idx]);
    return text;
}

/**
 * @brief registerThread - register new thread
 * @param name    - thread name
 * @param ID      - thread numeric ID
 * @param handler - thread handler
 * @return pointer to new threadinfo struct or NULL if failed
 */
threadinfo *registerThread(char *name, int ID, void *(*handler)(void *)){
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
    ti->handler = handler;
    snprintf(ti->name, THREADNAMEMAXLEN+1, "%s", name);
    ti->ID = ID;
    memset(&ti->mesg, 0, sizeof(ti->mesg));
    for(int i = 0; i < 2; ++i)
        pthread_mutex_init(&ti->mesg.mutex[i], NULL);
    if(pthread_create(&ti->thread, NULL, handler, (void*)ti)){
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
    for(int i = 0; i < 2; ++i){
        pthread_mutex_lock(&lptr->ti.mesg.mutex[i]);
        char *txt;
        while((txt = popmessage(&lptr->ti.mesg.text[i]))) FREE(txt);
        pthread_mutex_destroy(&lptr->ti.mesg.mutex[i]);
    }
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

#if 0
static void *handler(void *data){
    threadinfo *ti = (threadinfo*)data;
    while(1){
        char *got = getmesg(idxMOSI, &ti->mesg);
        if(got){
            green("%s got: %s\n", ti->name, got);
            FREE(got);
            addmesg(idxMISO, &ti->mesg, "received");
            addmesg(idxMISO, &ti->mesg, "need more");
        }
        usleep(100);
    }
    return NULL;
}

static void dividemessages(message *msg, char *longtext){
    char *copy = strdup(longtext), *saveptr = NULL;
    for(char *s = copy; ; s = NULL){
        char *nxt = strtok_r(s, " ", &saveptr);
        if(!nxt) break;
        addmesg(idxMOSI, msg, nxt);
    }
    FREE(copy);
}

static void procmesg(char *text){
    if(!text) return;
    char *nxt = strchr(text, ' ');
    if(!nxt){
        WARNX("Usage: cmd data, where cmd:\n"
              "\tnew threadname - create thread\n"
              "\tdel threadname - delete thread\n"
              "\tsend threadname data - send data to thread\n"
              "\tsend all data - send data to all\n");
        return;
    }
    *nxt++ = 0;
    if(strcasecmp(text, "new") == 0){
        registerThread(nxt, handler);
    }else if(strcasecmp(text, "del") == 0){
        if(killThread(nxt)) WARNX("Can't delete '%s'", nxt);
    }else if(strcasecmp(text, "send") == 0){
        text = strchr(nxt, ' ');
        if(!text){
            WARNX("send all/threadname data");
            return;
        }
        *text++ = 0;
        if(strcasecmp(nxt, "all") == 0){ // bcast
            threadlist *lptr = thelist;
            while(lptr){
                threadinfo *ti = &lptr->ti;
                lptr = lptr->next;
                green("Bcast send '%s' to thread '%s'\n", text, ti->name);
                dividemessages(&ti->mesg, text);
            }
        }else{ // single
            threadinfo *ti = findthread(nxt);
            if(!ti){
                WARNX("Thread '%s' not found", nxt);
                return;
            }
            green("Send '%s' to thread '%s'\n", text, nxt);
            dividemessages(&ti->mesg, text);
        }
    }
}

int main(){
    using_history();
    while(1){
        threadlist *lptr = thelist;
        while(lptr){
            threadinfo *ti = &lptr->ti;
            lptr = lptr->next;
            char *got;
            while((got = getmesg(idxMISO, &ti->mesg))){
                red("got from '%s': %s\n", ti->name, got);
                fflush(stdout);
                FREE(got);
            }
        }
        char *text = readline("mesg > ");
        if(!text) break; // ^D
        if(strlen(text) < 1) continue;
        add_history(text);
        procmesg(text);
        FREE(text);
    }
    return 0;
}

#endif


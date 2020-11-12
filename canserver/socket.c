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
#include "cmdlnopts.h"   // glob_pars
#include "proto.h"
#include "socket.h"
#include "term.h"

#include <arpa/inet.h>  // inet_ntop
#include <limits.h>     // INT_xxx
#include <netdb.h>      // addrinfo
#include <pthread.h>
#include <signal.h>     // pthread_kill
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>   // open
#include <sys/syscall.h> // syscall
#include <fcntl.h>      // open
#include <unistd.h>     // daemon
#include <usefull_macros.h>

// buffer size for received data
#define BUFLEN    (1024)
// Max amount of connections
#define BACKLOG   (30)

extern glob_pars *GP;

/*
 * Define global data buffers here
 */

/**************** COMMON FUNCTIONS ****************/
/**
 * wait for answer from socket
 * @param sock - socket fd
 * @return 0 in case of error or timeout, 1 in case of socket ready
 */
static int waittoread(int sock){
    fd_set fds;
    struct timeval timeout;
    int rc;
    timeout.tv_sec = 1; // wait not more than 1 second
    timeout.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    do{
        rc = select(sock+1, &fds, NULL, NULL, &timeout);
        if(rc < 0){
            if(errno != EINTR){
                WARN("select()");
                LOGWARN("waittoread(): select() error");
                return 0;
            }
            continue;
        }
        break;
    }while(1);
    if(FD_ISSET(sock, &fds)) return 1;
    return 0;
}

/**************** SERVER FUNCTIONS ****************/
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * Send data over socket
 * @param sock      - socket fd
 * @param textbuf   - zero-trailing buffer with data to send
 * @return amount of sent bytes
 */
static size_t send_data(int sock, char *textbuf){
    ssize_t Len = strlen(textbuf);
    if(Len != write(sock, textbuf, Len)){
        WARN("write()");
        LOGERR("send_data(): write() failed");
        return 0;
    }else LOGDBG("send_data(): sent '%s'", textbuf);
    return (size_t)Len;
}

#if 0
// search a first word after needle without spaces
static char* stringscan(char *str, char *needle){
    char *a;//, *e;
    char *end = str + strlen(str);
    a = strstr(str, needle);
    if(!a) return NULL;
    a += strlen(needle);
    while (a < end && (*a == ' ' || *a == '\r' || *a == '\t' || *a == '\r')) a++;
    if(a >= end) return NULL;
//    e = strchr(a, ' ');
//    if(e) *e = 0;
    return a;
}
#endif

static void *handle_socket(void *asock){
    FNAME();
    int sock = *((int*)asock);
    char buff[BUFLEN];
    ssize_t rd;
    while(1){
        if(!waittoread(sock)){ // no data incoming
            continue;
        }
        if(!(rd = read(sock, buff, BUFLEN-1))){
            DBG("Client closed socket");
            LOGDBG("Socket %d closed", sock);
            break;
        }
        DBG("Got %zd bytes", rd);
        if(rd < 0){ // error
            LOGDBG("Close socket %d: read=%d", sock, rd);
            DBG("Nothing to read from fd %d (ret: %zd)", sock, rd);
            break;
        }
        // add trailing zero to be on the safe side
        buff[rd] = 0;
        // now we should check what do user want
        // here we can process user data
        DBG("user send '%s'", buff);
        LOGDBG("user send '%s'", buff);
        if(GP->echo){
            if(!send_data(sock, buff)){
                WARN("Can't send data to user, some error occured");
            }
        }
        //pthread_mutex_lock(&mutex);
        char *ans = processCommand(buff); // run command parser
        if(ans){
            send_data(sock, ans);     // send answer
            FREE(ans);
        }
        //pthread_mutex_unlock(&mutex);
    }
    LOGDBG("Socket %d closed", sock);
    DBG("Socket closed");
    close(sock);
    pthread_exit(NULL);
    return NULL;
}

// main socket server
static void *server(void *asock){
    LOGMSG("server(): getpid: %d, tid: %lu",getpid(), syscall(SYS_gettid));
    int sock = *((int*)asock);
    if(listen(sock, BACKLOG) == -1){
        LOGERR("server(): listen() failed");
        WARN("listen");
        return NULL;
    }
    while(1){
        socklen_t size = sizeof(struct sockaddr_in);
        struct sockaddr_in their_addr;
        int newsock;
        if(!waittoread(sock)) continue;
        newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
        if(newsock <= 0){
            LOGERR("server(): accept() failed");
            WARN("accept()");
            continue;
        }
        pthread_t handler_thread;
        if(pthread_create(&handler_thread, NULL, handle_socket, (void*) &newsock)){
            LOGERR("server(): pthread_create() failed");
            WARN("pthread_create()");
        }else{
            LOGDBG("server(): listen thread created");
            DBG("Thread created, detouch");
            pthread_detach(handler_thread); // don't care about thread state
        }
    }
    LOGERR("server(): UNREACHABLE CODE REACHED!");
}

// data gathering & socket management
static void daemon_(int sock){
    if(sock < 0) return;
    double tgot = 0.;
    char *devname = find_device();
    if(!devname){
        LOGERR("Can't find serial device");
        ERRX("Can't find serial device");
    }
    pthread_t sock_thread;
    if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
        LOGERR("daemon_(): pthread_create() failed");
        ERR("pthread_create()");
    }
    do{
        if(pthread_kill(sock_thread, 0) == ESRCH){ // died
            WARNX("Sockets thread died");
            LOGERR("Sockets thread died");
            pthread_join(sock_thread, NULL);
            if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
                LOGERR("daemon_(): new pthread_create() failed");
                ERR("pthread_create()");
            }
        }
        usleep(1000); // sleep a little or thread's won't be able to lock mutex
        if(dtime() - tgot < T_INTERVAL) continue;
        tgot = dtime();
        /*
         * INSERT CODE HERE
         * Gather data (poll_device)
         */
        // copy temporary buffers to main
        //pthread_mutex_lock(&mutex);
        int fd = open(devname, O_RDONLY);
        if(fd == -1){
            WARN("open()");
            LOGWARN("Device %s is absent", devname);
            FREE(devname);
            double t0 = dtime();
            while(dtime() - t0 < 5.){
                if((devname = find_device())) break;
                usleep(1000);
            }
            if(!devname){
                LOGERR("Can't open serial device, kill myself");
                ERRX("Can't open device, kill myself");
            }else LOGMSG("Change device to %s", devname);
        }else close(fd);
        /*
         * INSERT CODE HERE
         * fill global data buffers
         */
        //pthread_mutex_unlock(&mutex);
    }while(1);
    LOGERR("daemon_(): UNREACHABLE CODE REACHED!");
}

/**
 * Run daemon service
 */
void daemonize(char *port){
    FNAME();
    int sock = -1;
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if(getaddrinfo("127.0.0.1", port, &hints, &res) != 0){ // accept only local connections
        LOGERR("daemonize(): getaddrinfo() failed");
        ERR("getaddrinfo");
    }
    struct sockaddr_in *ia = (struct sockaddr_in*)res->ai_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ia->sin_addr), str, INET_ADDRSTRLEN);
    // loop through all the results and bind to the first we can
    for(p = res; p != NULL; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            LOGWARN("daemonize(): socket() failed");
            WARN("socket");
            continue;
        }
        int reuseaddr = 1;
        if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
            LOGERR("daemonize(): setsockopt() failed");
            ERR("setsockopt");
        }
        if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
            close(sock);
            LOGERR("daemonize(): bind() failed");
            WARN("bind");
            continue;
        }
        break; // if we get here, we have a successfull connection
    }
    if(p == NULL){
        LOGERR("daemonize(): failed to bind socket, exit");
        // looped off the end of the list with no successful bind
        ERRX("failed to bind socket");
    }
    freeaddrinfo(res);
    daemon_(sock);
    close(sock);
    LOGERR("daemonize(): UNREACHABLE CODE REACHED!");
    signals(0);
}


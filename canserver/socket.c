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
#include "processmotors.h"
#include "proto.h"
#include "socket.h"
#include "term.h"

#include <arpa/inet.h>  // inet_ntop
#include <sys/ioctl.h>
#include <limits.h>     // INT_xxx
#include <netdb.h>      // addrinfo
#include <poll.h>
#include <pthread.h>
#include <signal.h>     // pthread_kill
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h> // syscall
#include <unistd.h>     // daemon
#include <usefull_macros.h>

// buffer size for received data
#define BUFLEN    (1024)
// Max amount of connections
#define BACKLOG   (30)

message ServerMessages = {0};

/**************** SERVER FUNCTIONS ****************/
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/**
 * Send data over socket (and add trailing '\n' if absent)
 * @param sock      - socket fd
 * @param textbuf   - zero-trailing buffer with data to send
 * @return amount of sent bytes
 */
static size_t send_data(int sock, const char *textbuf){
    ssize_t Len = strlen(textbuf);
    if(Len != write(sock, textbuf, Len)){
        WARN("write()");
        LOGERR("send_data(): write() failed");
        return 0;
    }else LOGDBG("send_data(): sent '%s'", textbuf);
    if(textbuf[Len-1] != '\n') Len += write(sock, "\n", 1);
    return (size_t)Len;
}

/**
 * @brief handle_socket - read and process data from socket
 * @param sock - socket fd
 * @return 0 if all OK, 1 if socket closed
 */
static int handle_socket(int sock){
    FNAME();
    char buff[BUFLEN];
    ssize_t rd = read(sock, buff, BUFLEN-1);
    if(rd < 1){
        DBG("read() == %zd", rd);
        return 1;
    }
    // add trailing zero to be on the safe side
    buff[rd] = 0;
    // now we should check what do user want
    // here we can process user data
    DBG("user %d send '%s'", sock, buff);
    LOGDBG("user %d send '%s'", sock, buff);
    if(GP->echo){
        send_data(sock, buff);
    }
    //pthread_mutex_lock(&mutex);
    const char *ans = processCommand(buff); // run command parser
    if(ans){
        send_data(sock, ans);   // send answer
    }
    //pthread_mutex_unlock(&mutex);
    return 0;
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
    int nfd = 1;
    // max amount of opened fd (+1 for server socket)
#define MAX_FDS (3)
    struct pollfd poll_set[MAX_FDS];
    memset(poll_set, 0, sizeof(poll_set));
    poll_set[0].fd = sock;
    poll_set[0].events = POLLIN;
    while(1){
        poll(poll_set, nfd, 1); // poll for 1ms
        for(int fdidx = 0; fdidx < nfd; ++fdidx){ // poll opened FDs
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            poll_set[fdidx].revents = 0;
            if(fdidx){ // client
                int fd = poll_set[fdidx].fd;
                //int nread = 0;
                //ioctl(fd, FIONREAD, &nread);
                if(handle_socket(fd)){ // socket closed - remove it from list
                    close(fd);
                    DBG("Client with fd %d closed", fd);
                    LOGMSG("Client %d disconnected", fd);
                    // move last to free space
                    poll_set[fdidx] = poll_set[nfd - 1];
                    //for(int i = fdidx; i < nfd-1; ++i)
                    //    poll_set[i] = poll_set[i + 1];
                    --nfd;
                }
            }else{ // server
                socklen_t size = sizeof(struct sockaddr_in);
                struct sockaddr_in their_addr;
                int newsock = accept(sock, (struct sockaddr*)&their_addr, &size);
                if(newsock <= 0){
                    LOGERR("server(): accept() failed");
                    WARN("accept()");
                    continue;
                }
                struct in_addr ipAddr = their_addr.sin_addr;
                char str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &ipAddr, str, INET_ADDRSTRLEN);
                DBG("Connection from %s, give fd=%d", str, newsock);
                LOGMSG("Got connection from %s, fd=%d", str, newsock);
                if(nfd == MAX_FDS){
                    LOGWARN("Max amount of connections: disconnect %s (%d)", str, newsock);
                    send_data(newsock, "Max amount of connections reached!\n");
                    WARNX("Limit of connections reached");
                    close(newsock);
                }else{
                    memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                    poll_set[nfd].fd = newsock;
                    poll_set[nfd].events = POLLIN;
                    ++nfd;
                }
            }
        } // endfor
        char *srvmesg = mesgGetText(&ServerMessages); // broadcast messages to all clients
        if(srvmesg){ // send broadcast message to all clients or throw them to /dev/null
            for(int fdidx = 1; fdidx < nfd; ++fdidx){
                send_data(poll_set[fdidx].fd, srvmesg);
            }
            FREE(srvmesg);
        }
    }
    LOGERR("server(): UNREACHABLE CODE REACHED!");
}

// data gathering & socket management
static void daemon_(int sock){
    if(sock < 0) return;
    pthread_t sock_thread, canserver_thread;
    if(pthread_create(&sock_thread, NULL, server, (void*) &sock) ||
       pthread_create(&canserver_thread, NULL, CANserver, NULL)){
        LOGERR("daemon_(): pthread_create() failed");
        ERR("pthread_create()");
    }
    do{
        if(pthread_kill(sock_thread, 0) == ESRCH){ // died
            WARNX("Sockets thread died");
            LOGERR("Sockets thread died");
            pthread_join(sock_thread, NULL);
            if(pthread_create(&sock_thread, NULL, server, (void*) &sock)){
                LOGERR("daemon_(): new pthread_create(sock_thread) failed");
                ERR("pthread_create(sock_thread)");
            }
        }
        if(pthread_kill(canserver_thread, 0) == ESRCH){
            WARNX("CANserver thread died");
            LOGERR("CANserver thread died");
            pthread_join(canserver_thread, NULL);
            if(pthread_create(&canserver_thread, NULL, CANserver, NULL)){
                LOGERR("daemon_(): new pthread_create(canserver_thread) failed");
                ERR("pthread_create(canserver_thread)");
            }
        }
        usleep(1000); // sleep a little or thread's won't be able to lock mutex
        // copy temporary buffers to main
        //pthread_mutex_lock(&mutex);
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


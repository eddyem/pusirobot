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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>   // wait
#include <sys/prctl.h>  // prctl
#include <unistd.h>     // daemon
#include <usefull_macros.h>

#include "aux.h"
#include "cmdlnopts.h"
#include "socket.h"

glob_pars *GP; // non-static: to use in outhern functions

void signals(int signo){
    //restore_tty();
    unlink(GP->pidfile);
    LOGERR("Exit with status %d", signo);
    exit(signo);
}

void iffound_default(pid_t pid){
    LOGWARN("Another copy of this process found, pid=%d. Exit.", pid);
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

int main(int argc, char **argv){
#ifndef EBUG
    char *self = strdup(argv[0]);
#endif
    initial_setup();
    GP = parse_args(argc, argv);
    if(!GP->device && !GP->vid && !GP->pid) red("No device PID/VID/filename given, try to find firs comer!\n");
  /*  if(GP->checkfile){ // just check and exit
        return parse_data_file(GP->checkfile, 0);
    }*/
    char *dev = find_device();
    red("dev = %s\n", dev);
    if(!dev) ERRX("Serial device not found!");
    FREE(dev);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    if(GP->logfile){
        Cl_loglevel lvl = LOGLEVEL_ERR;
        int v = GP->verb;
        while(v--){ // increase loglevel
            if(++lvl == LOGLEVEL_ANY) break;
        }
        green("Log level: %d\n", lvl);
        OPENLOG(GP->logfile, lvl);
    }
    #ifndef EBUG
    if(daemon(1, 0)){
        ERR("daemon()");
    }
    check4running(self, GP->pidfile);
    FREE(self);
    while(1){ // guard for dead processes
        pid_t childpid = fork();
        if(childpid){
            LOGDBG("create child with PID %d", childpid);
            DBG("Created child with PID %d\n", childpid);
            wait(NULL);
            LOGDBG("child %d died", childpid);
            WARNX("Child %d died\n", childpid);
            sleep(1);
        }else{
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break; // go out to normal functional
        }
    }
    #endif

    /*
     * INSERT CODE HERE
     * connection check & device validation
     */
    //if(!G->terminal) signals(15); // there's not main controller connected to given terminal
    daemonize(GP->port);
    return 0;
}
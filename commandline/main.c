/*
 * This file is part of the usefull_macros project.
 * Copyright 2018 Edward V. Emelianoff <eddy@sao.ru>.
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
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <usefull_macros.h>

#include "canbus.h"
#include "canopen.h"
#include "cmdlnopts.h"
#include "pusirobot.h"

static glob_pars *GP = NULL;  // for GP->pidfile need in `signals`

void signals(int sig){
    putlog("Exit with status %d", sig);
    restore_console();
    if(GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    canbus_close();
    exit(sig);
}

void iffound_default(pid_t pid){
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

int main(int argc, char *argv[]){
    initial_setup();
    char *self = strdup(argv[0]);
    GP = parse_args(argc, argv);
    check4running(self, GP->pidfile);
    free(self);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    if(GP->NodeID != 1){
        if(GP->NodeID < 1 || GP->NodeID > 127) ERRX("Node ID should be a number from 1 to 127");
    }
    if(GP->logfile) openlogfile(GP->logfile);
    putlog(("Start application..."));
    putlog("Try to open CAN bus device %s", GP->device);
    setserialspeed(GP->serialspeed);
    if(canbus_open(GP->device)){
        putlog("Can't open %s @ speed %d. Exit.", GP->device, GP->serialspeed);
        signals(1);
    }
    if(canbus_setspeed(GP->canspeed)){
        putlog("Can't set CAN speed %d. Exit.", GP->canspeed);
        signals(2);
    }

    //setup_con();
    // print current position and state
    int64_t i64;
    if(INT64_MIN != (i64 = SDO_read(&DEVSTATUS, GP->NodeID)))
        green("DEVSTATUS=%d\n", (int)i64);
    if(INT64_MIN != (i64 = SDO_read(&POSITION, GP->NodeID)))
        green("CURPOS=%d\n", (int)i64);
    if(INT64_MIN != (i64 = SDO_read(&MAXSPEED, GP->NodeID)))
        green("MAXSPEED=%d\n", (int)i64);

#if 0
    CANmesg m;
    double t = dtime() - 10.;
    while(1){
        m.ID = 0; // read all
        if(!canbus_read(&m)){
            showM(&m);
            SDO *x = parseSDO(&m);
            if(x){
                printf("Get SDO, NID=%d, CCS=%d, idx=%d, subidx=%d, datalen=%d\n", x->NID, x->ccs, x->index, x->subindex, x->datalen);
            }
        }
        if(dtime() - t > 5.){
            t = dtime();
            green("Try to get status.. ");
            uint8_t s;
            if(SDO_readByte(0x6001, 0, &s, 1)) red("Err\n");
            else printf("got: 0x%X\n", s);
        }
    }
#endif
    signals(0);
    return 0;
}

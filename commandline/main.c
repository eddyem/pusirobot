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
#include "dataparser.h"
#include "pusirobot.h"
#include "verblog.h"

static glob_pars *GP = NULL;  // for GP->pidfile need in `signals`
static uint8_t ID = 0;
static uint8_t devstat = 0; // device status after chkstat()

// default signal handler
void signals(int sig){
    LOGERR("Exit with status %d", sig);
    DBG("Exit with status %d", sig);
    if(GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    canbus_close();
    exit(sig);
}

void iffound_default(pid_t pid){
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

// error state check
static inline void chkerr(int64_t es){
    if(!es) return;
    red("ERRSTATE=%d\n", es);
    uint8_t s = (uint8_t)es;
    for(uint8_t i = 0; i < 8; ++i){
        const char *msg = errname(s, i);
        if(msg) red("\t%s\n", msg);
    }
    if(!GP->clearerr) ERRX("Error status is not zero");
    if(SDO_write(&ERRSTATE, ID, s) || 0 != (es = SDO_read(&ERRSTATE, ID))){
        ERRX("Can't clean error status");
    }
}

// device status check
static inline void chkstat(int64_t es){
    if(es) red("DEVSTATUS=%d\n", (int)es);
    else message(1, "DEVSTATUS=0");
    devstat = (uint8_t)es;
    if(devstat){
        for(uint8_t i = 0; i < 8; ++i){
            const char *msg = devstatus(devstat, i);
            if(msg) red("\t%s\n", msg);
        }
        if(devstat != BUSY_STATE && GP->clearerr && (SDO_write(&DEVSTATUS, ID, devstat) || 0 != (es = SDO_read(&DEVSTATUS, ID)))){
            ERRX("Can't clean device status");
        }else devstat = (uint8_t)es;
        if(es && es != BUSY_STATE) ERRX("Can't work in this state"); // DIE if !busy
    }
}

/*
// setup microstepping
static inline void setusteps(int64_t es){
    DBG("es=%zd", es);
    int us = GP->microsteps;
    if(us > 0){
        if(us == 1) us = 0; // PusiRobot driver needs value 0 for microstepping 1!
        if(us != (int) es){ // change microstepping
        DBG("Try to change microsteps");
        if(SDO_write(&MICROSTEPS, ID, us) || INT64_MIN == (es = SDO_read(&MICROSTEPS, ID)))
            ERRX("Can't change microstepping");
        }
    }
    microstepping = es > 0 ? (uint16_t) es : 1;
    green("MICROSTEPPING=%u\n", microstepping);
}*/

// setup maximal speed
static inline void setmaxspd(int64_t es){
    DBG("abs=%d, rel=%d", GP->absmove, GP->relmove);
    if(es == 0 && (GP->absmove != INT_MIN || GP->relmove != INT_MIN) && (GP->maxspeed == INT_MIN || GP->maxspeed == 0))
        ERRX("Can't move when MAXSPEED==0");
    if(GP->maxspeed != INT_MIN){
        GP->maxspeed *= SPEED_MULTIPLIER;
        if(GP->maxspeed < MAX_SPEED_MIN|| GP->maxspeed > MAX_SPEED_MAX)
            ERRX("MAXSPEED should be from %d to %d", MAX_SPEED_MIN / SPEED_MULTIPLIER, MAX_SPEED_MAX / SPEED_MULTIPLIER);
        DBG("Try to change max speed");
        if(SDO_write(&MAXSPEED, ID, GP->maxspeed) || INT64_MIN == (es = SDO_read(&MAXSPEED, ID)))
            ERRX("Can't change max speed");
    }
    if(es) message(1, "MAXSPEED=%d", (int)es);
    else red("MAXSPEED=0\n");
}

// get ESW values
static inline void gpioval(int64_t es){
    uint16_t v = (uint16_t) es;
    if(INT64_MIN == (es = SDO_read(&EXTENABLE, ID))){
        WARNX("Can't read limit switches configuration");
        return;
    }
    for(int i = 1; i < 4; ++i){
        if(0 == (es & 1<<(i-1))) continue; // endswitch disabled
        printf("LIM%d=%d\n", i, (EXTACTIVE(i, v)));
    }
}

// show values of all parameters from dicentries.in
static inline void showAllPars(){
    green("\nParameters' values:\n");
    for(int i = 0; i < DEsz; ++i){
        const SDO_dic_entry *entry = allrecords[i];
        int64_t val = SDO_read(entry, GP->NodeID);
        if(val == INT64_MIN){
            WARNX("Can't read value of SDO 0x%04X/%d (%s)",
                  entry->index, entry->subindex, entry->name);
            continue;
        }
        printf("# %s\n0x%04X, %d, %ld (0x%X)\n", entry->name, entry->index,
               entry->subindex, val, (uint32_t)val);
        fflush(stdout);
    }
    printf("\n\n");
}

// wait while device is in busy state
static inline void wait_busy(){
    int errctr = 0;
    int64_t ans;
    int i = 0;
    const char symbols[] = "|/-\\";
    printf("Waiting... ");
    do{
        printf("%c\b", symbols[i]);
        fflush(stdout);
        if(++i > 3) i = 0;
        if(INT64_MIN == (ans = SDO_read(&DEVSTATUS, ID))){
            if(++errctr > 10) ERRX("Can't read device status");
        }else errctr = 0;
        if(ans == BUSY_STATE){
            usleep(50000);
        }else{
            chkstat(ans);
            break;
        }
    }while(1);
    printf("\n");
}

int main(int argc, char *argv[]){
    sl_init();
    GP = parse_args(argc, argv);
    if(GP->verblevel) maxmesglevl(GP->verblevel);
    if(GP->checkfile){ // just check and exit
        char **c = GP->checkfile;
        int r = 0;
        while(*c){
            message(1, "\nCheck data file %s", *c);
            r += parse_data_file(*c, 0);
            ++c;
        }
        return r;
    }
    if(GP->NodeID != 1){
        if(GP->NodeID < 1 || GP->NodeID > 127) ERRX("Node ID should be a number from 1 to 127");
    }
    DBG("builtin: %d", __builtin_popcount(GP->microsteps));
    if(GP->microsteps > 0 && (1 != __builtin_popcount(GP->microsteps))) // __builtin_popcount - amount of non-zero bits in uint
        ERRX("Wrong microstepping settings, should be 2^(0..8)");
    if(GP->absmove != INT_MIN || GP->relmove != INT_MIN){ // wanna move
        if(GP->absmove != INT_MIN && GP->relmove != INT_MIN)
            ERRX("ABSMOVE and RELMOVE can't be used together");
        if(GP->maxspeed == 0)
            ERRX("Set non-zero MAXSPEED");
    }
    if(GP->enableESW && GP->disableESW) ERRX("Enable & disable ESW can't meet together");

    sl_check4running(NULL, GP->pidfile);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z

    if(GP->logfile){
        sl_loglevel_e l = LOGLVL + GP->verblevel;
        if(l > LOGLEVEL_ANY) l = LOGLEVEL_ANY;
        OPENLOG(GP->logfile, l, 1);
        LOGMSG(("Start application..."));
        LOGMSG("Try to open CAN bus device %s", GP->device);
    }
    setserialspeed(GP->serialspeed);
    if(canbus_open(GP->device)){
        LogAndErr("Can't open %s @ speed %d. Exit.", GP->device, GP->serialspeed);
    }
    if(canbus_setspeed(GP->canspeed)){
        LogAndErr("Can't set CAN speed %d. Exit.", GP->canspeed);
    }

    // print current position and state
    int64_t i64;
    ID = GP->NodeID;
#define getSDOe(SDO, fn, e)  do{if(INT64_MIN != (i64 = SDO_read(&SDO, ID))) fn(i64); else LogAndWarn(e); }while(0)
#define getSDOw(SDO, fn, e)  do{if(INT64_MIN != (i64 = SDO_read(&SDO, ID))) fn(i64); else LogAndErr(e); }while(0)

#define Mesg(...)
//#define Mesg(...)    green(__VA_ARGS__)
//double d0 = dtime();

    // get mircostepping (need this to properly calculate current position and move
    /*if(!GP->fracsteps){
        getSDOe(MICROSTEPS, setusteps, "Can't get microstepping");
        Mesg("MICROSTEPS: %g\n", dtime() - d0);
    }*/
    // enable limit switches
    if(GP->enableESW){
        if(SDO_write(&EXTENABLE, ID, GP->enableESW)){
            WARNX("Error when trying to enable limit switches");
            if(GP->absmove || GP->relmove) signals(-1);
        }
        Mesg("EXTENABLE: %g\n", dtime() - d0);
    }
    // disable limit switches
    if(GP->disableESW){
        if(SDO_write(&EXTENABLE, ID, 0)){
            WARNX("Error when trying to disable limit switches");
            if(GP->absmove || GP->relmove) signals(-1);
        }
    }
    //int64_t es = SDO_read(&EXTENABLE, ID);
    //green("LIMITSW=%lld\n", es);
    if(!GP->quick){
        // check error status
        getSDOe(ERRSTATE, chkerr, "Can't get error status");
        Mesg("ERRSTATE: %g\n", dtime() - d0);
        // get current position
        if(INT64_MIN != (i64 = SDO_read(&POSITION, ID))){
            //int enc = (int)i64;
            message(1, "CURENCODER=%d", i64);
            //message(1, "CURSTEPS=%.2f", i64 / ((double)microstepping));
        }
        else WARNX("Can't read current position");
        Mesg("CURPOS: %g\n", dtime() - d0);
        // get limit switches values
        getSDOe(GPIOVAL, gpioval, "Can't read GPIO values");
        Mesg("GPIOVAL: %g\n", dtime() - d0);
        // get motor power status
        if(INT64_MIN != (i64 = SDO_read(&ENABLE, ID))){
            if(i64) message(1, "ENABLE=1");
            else message(1, "ENABLE=0");
            Mesg("Status: %g\n", dtime() - d0);
        }
        // get max speed
        getSDOe(MAXSPEED, setmaxspd, "Can't read max speed");
        Mesg("MAXSPEED: %g\n", dtime() - d0);
        i64 = SDO_read(&MICROSTEPS, ID);
        if(i64 == INT64_MIN) LogAndWarn("Can't get microstepping value");
        else message(2, "MICROSTEPS=%u", i64);
    }
    i64 = SDO_read(&ENCRESOL, ID);
    if(i64 == INT64_MIN){ /* LogAndWarn("Can't get encoder resolution value"); */}
    else message(2, "ENCRESOL=%u", 1 << i64);
    if(GP->absmove != INT_MIN || GP->relmove != INT_MIN || !GP->quick || GP->wait){
        // check device status
        getSDOe(DEVSTATUS, chkstat, "Can't get device status");
        Mesg("DEVSTATUS: %g\n", dtime() - d0);
        if(devstat == BUSY_STATE && GP->wait) wait_busy();
    }
    // stop motor
    if(GP->stop){
        if(SDO_write(&STOP, ID, 1)) ERRX("Can't stop motor");
        Mesg("STOP: %g\n", dtime() - d0);
    }
    // turn off motor power
    if(GP->disable){
        if(SDO_write(&ENABLE, ID, 0)) ERRX("Can't disable motor");
        Mesg("DISABLE: %g\n", dtime() - d0);
    }
    // zero position
    if(GP->zeropos){
        if(SDO_write(&POSITION, ID, 0))
            ERRX("Can't clear position counter");
            Mesg("POSITION: %g\n", dtime() - d0);
    }
    // show values of all known CanOpen parameters
    if(GP->showpars){
        showAllPars();
        Mesg("showAllPars: %g\n", dtime() - d0);
    }
    // send values from external configuration file
    if(GP->parsefile){
        char **p = GP->parsefile;
        while(*p){
            message(1, "Try to parse %s and send SDOs to device", *p);
            parse_data_file(*p, GP->NodeID);
            Mesg("parse_data_file: %g\n", dtime() - d0);
            ++p;
        }
    }
    if(GP->absmove != INT_MIN){
        if(devstat == BUSY_STATE) ERRX("Can't move in BUSY state");
        SDO_write(&ENABLE, ID, 1);
        if(SDO_write(&ABSSTEPS, ID, GP->absmove))
            ERRX("Can't move to absolute position %d", GP->absmove);
    }
    if(GP->relmove != INT_MIN && GP->relmove){
        if(devstat == BUSY_STATE) ERRX("Can't move in BUSY state");
        uint8_t dir = 1;
        SDO_write(&ENABLE, ID, 1);
        if(GP->relmove < 0){ // negative direction
            dir = 0;
            GP->relmove = -GP->relmove;
        }
        if(SDO_write(&ROTDIR, ID, dir) || INT64_MIN == (i64 = SDO_read(&ROTDIR, ID)))
            ERRX("Can't change rotation direction");
        if(SDO_write(&RELSTEPS, ID, GP->relmove))
            ERRX("Can't move to relative position %d", GP->relmove);
        Mesg("RelMove: %g\n", dtime() - d0);
    }
    if(GP->wait) wait_busy();
#undef getSDOe

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

/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"
#include "pusirobot.h"

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars  G;

// default PID filename:
#define DEFAULT_PIDFILE     "/tmp/steppersmng.pid"
#define DEFAULT_PORTDEV     "/dev/ttyUSB0"
#define DEFAULT_SER_SPEED   115200
//            DEFAULTS
// default global parameters
static glob_pars const Gdefault = {
    .device = DEFAULT_PORTDEV,
    .pidfile = DEFAULT_PIDFILE,
    .serialspeed = DEFAULT_SER_SPEED,
    .canspeed = 0,  // don't set
    .logfile = NULL, // don't save logs
    .NodeID = 1,    // default node ID = 1
    .absmove = INT_MIN,
    .relmove = INT_MIN,
    .microsteps = -1,
    .maxspeed = INT_MIN,
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
static sl_option_t cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("serial device name (default: " DEFAULT_PORTDEV ")")},
    {"canspd",  NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.canspeed),  _("CAN bus speed")},
    {"serialspd",NEED_ARG,  NULL,   't',    arg_int,    APTR(&G.serialspeed),_("serial (tty) device speed (default: " STR(DEFAULT_SER_SPEED) ")")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("file to save logs")},
    {"pidfile", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"nodeid",  NEED_ARG,   NULL,   'I',    arg_int,    APTR(&G.NodeID),    _("node ID (1..127)")},
    {"microsteps", NEED_ARG,NULL,   'u',    arg_int,    APTR(&G.microsteps),_("set microstepping (0..256)")},
    {"rel",     NEED_ARG,   NULL,   'r',    arg_int,    APTR(&G.relmove),   _("move to relative position (in encoder ticks)")},
    {"abs",     NEED_ARG,   NULL,   'a',    arg_int,    APTR(&G.absmove),   _("move to absolute position (in encoder ticks)")},
    {"maxspd",  NEED_ARG,   NULL,   'm',    arg_int,    APTR(&G.maxspeed),  _("maximal motor speed (enc ticks per second)")},
    {"stop",    NO_ARGS,    NULL,   'S',    arg_int,    APTR(&G.stop),      _("stop motor")},
    {"clearerr",NO_ARGS,    NULL,   'c',    arg_int,    APTR(&G.clearerr),  _("clear errors")},
    {"zeropos", NO_ARGS,    NULL,   '0',    arg_int,    APTR(&G.zeropos),   _("set current position to zero")},
    {"parse",   MULT_PAR,   NULL,   'p',    arg_string, APTR(&G.parsefile), _("file[s] with SDO data to send to device")},
    {"check",   MULT_PAR,   NULL,   'k',    arg_string, APTR(&G.checkfile), _("check SDO data file")},
    {"disable", NO_ARGS,    NULL,   'D',    arg_int,    APTR(&G.disable),   _("disable motor")},
    {"readvals",NO_ARGS,    NULL,   'R',    arg_int,    APTR(&G.showpars),  _("read values of used parameters")},
    {"enablesw",NEED_ARG,   NULL,   'E',    arg_int,    APTR(&G.enableESW), _("enable end-switches with given mask")},
    {"disablesw",NO_ARGS,   NULL,   'A',    arg_int,    APTR(&G.disableESW),_("disable end-switches")},
    {"wait",    NO_ARGS,    NULL,   'w',    arg_int,    APTR(&G.wait),      _("wait while motor is busy")},
    {"quick",   NO_ARGS,    NULL,   'q',    arg_int,    APTR(&G.quick),     _("directly send command without getting status")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verblevel), _("verbosity level for logging (each -v increases level)")},
   end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    sl_helpstring(helpstring);
    // parse arguments
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(help) sl_showhelp(-1, cmdlnopts);
    if(argc > 0){
        WARNX("Extra arguments: ");
        for (i = 0; i < argc; i++) fprintf(stderr, "\t%s\n", argv[i]);
        sl_showhelp(-1, cmdlnopts);
    }
    return &G;
}


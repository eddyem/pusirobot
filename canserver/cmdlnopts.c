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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <usefull_macros.h>

#include "cmdlnopts.h"

/*
 * here are global parameters initialisation
 */
int help;
static glob_pars  G;

// default values for Gdefault & help
#define DEFAULT_PORT        "4444"
#define DEFAULT_PIDFILE     "/tmp/canserver.pid"


//            DEFAULTS
// default global parameters
glob_pars const Gdefault = {
    .device = NULL,
    .port = DEFAULT_PORT,
    .terminal = 0,
    .echo = 0,
    .logfile = NULL,
    .rest_pars = NULL,
    .rest_pars_num = 0
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
*/
myoption cmdlnopts[] = {
// common options
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"device",  NEED_ARG,   NULL,   'i',    arg_string, APTR(&G.device),    _("serial device name (default: none)")},
    {"port",    NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.port),      _("network port to connect (default: " DEFAULT_PORT ")")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("save logs to file (default: none)")},
    {"echo",    NO_ARGS,    NULL,   'e',    arg_int,    APTR(&G.echo),      _("echo users commands back")},
    {"pidfile", NEED_ARG,   NULL,   0,      arg_string, APTR(&G.pidfile),   _("name of PID file (default: " DEFAULT_PIDFILE ")")},
    {"verbose", NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verb),      _("increase verbosity level of log file (each -v increased by 1)")},
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
    // format of help: "Usage: progname [args]\n"
    change_helpstring("Usage: %s [args]\n\n\tWhere args are:\n");
    // parse arguments
    parseargs(&argc, &argv, cmdlnopts);
    if(help) showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = calloc(argc, sizeof(char*));
        for (i = 0; i < argc; i++)
            G.rest_pars[i] = strdup(argv[i]);
    }
    return &G;
}

Cl_log *globlog = NULL;

/**
 * @brief Cl_createlog - create log file: init mutex, test file open ability
 * @param logpath - path to log file
 * @param level   - lowest message level (e.g. LOGLEVEL_ERR won't allow to write warn/msg/dbg)
 * @return allocated structure (should be free'd later by Cl_deletelog) or NULL
 */
Cl_log *Cl_createlog(const char *logpath, Cl_loglevel level){
    if(level < LOGLEVEL_NONE || level > LOGLEVEL_ANY) return NULL;
    if(!logpath) return NULL;
    FILE *logfd = fopen(logpath, "a");
    if(!logfd){
        WARN("Can't open log file");
        return NULL;
    }
    fclose(logfd);
    Cl_log *log = MALLOC(Cl_log, 1);
    if(pthread_mutex_init(&log->mutex, NULL)){
        WARN("Can't init log mutex");
        FREE(log);
        return NULL;
    }
    log->logpath = strdup(logpath);
    if(!log->logpath){
        WARN("strdup()");
        FREE(log);
        return NULL;
    }
    log->loglevel = level;
    return log;
}

void Cl_deletelog(Cl_log **log){
    if(!log || !*log) return;
    FREE((*log)->logpath);
    FREE(*log);
}

/**
 * @brief Cl_putlog - put message to log file with/without timestamp
 * @param timest - ==1 to put timestamp
 * @param log - pointer to log structure
 * @param lvl - message loglevel (if lvl > loglevel, message won't be printed)
 * @param fmt - format and the rest part of message
 * @return amount of symbols saved in file
 */
int Cl_putlogt(int timest, Cl_log *log, Cl_loglevel lvl, const char *fmt, ...){
    if(!log || !log->logpath) return 0;
    if(lvl > log->loglevel) return 0;
    if(pthread_mutex_lock(&log->mutex)){
        WARN("Can't lock log mutex");
        return 0;
    }
    int i = 0;
    FILE *logfd = fopen(log->logpath, "a+");
    if(!logfd) goto rtn;
    if(timest){
        char strtm[128];
        time_t t = time(NULL);
        struct tm *curtm = localtime(&t);
        strftime(strtm, 128, "%Y/%m/%d-%H:%M:%S", curtm);
        i = fprintf(logfd, "%s", strtm);
    }
    i += fprintf(logfd, "\t");
    va_list ar;
    va_start(ar, fmt);
    i += vfprintf(logfd, fmt, ar);
    va_end(ar);
    fseek(logfd, -1, SEEK_CUR);
    char c;
    ssize_t r = fread(&c, 1, 1, logfd);
    if(1 == r){ // add '\n' if there was no newline
        if(c != '\n') i += fprintf(logfd, "\n");
    }
    fclose(logfd);
rtn:
    pthread_mutex_unlock(&log->mutex);
    return i;
}

/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.h - comand line options for parceargs
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

#pragma once
#ifndef CMDLNOPTS_H__
#define CMDLNOPTS_H__

#ifndef STR
#define STR(x) _S_(x)
#define _S_(x) #x
#endif

/*
 * here are some typedef's for global data
 */
typedef struct{
    char *device;           // serial device name
    char *pidfile;          // name of PID file
    char *logfile;          // logging to this file
    char *parsefile;        // file to parse
    char *checkfile;        // SDO data filename to check
    int canspeed;           // CAN bus speed
    int serialspeed;        // serial device speed (CAN-bus to USB)
    int NodeID;             // node ID to work with
    int absmove;            // absolute position to move
    int relmove;            // relative position to move
    int microsteps;         // set microstepping
    int maxspeed;           // max speed
    int stop;               // stop motor
    int clearerr;           // try to clear errors
    int zeropos;            // set position to zero
    int disable;            // disable motor
    int showpars;           // show values of some parameters
} glob_pars;


glob_pars *parse_args(int argc, char **argv);

#endif // CMDLNOPTS_H__

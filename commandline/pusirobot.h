/*
 * This file is part of the stepper project.
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
#pragma once
#ifndef PUSIROBOT_H__
#define PUSIROBOT_H__

#include <stdint.h>

// entry of SDO dictionary
typedef struct{
    uint16_t index;     // SDO index
    uint8_t subindex;   // SDO subindex
    uint8_t datasize;   // data size: 1,2,3 or 4 bytes
    uint8_t issigned;   // signess: if issigned==1, then signed, else unsigned
} SDO_dic_entry;

#ifndef DICENTRY
#define DICENTRY(name, idx, sidx, sz, s)  extern const SDO_dic_entry name;
#endif

// heartbeat time
DICENTRY(HEARTBTTIME,   0x1017, 0, 2, 0)
// node ID
DICENTRY(NODEID,        0x2002, 0, 1, 0)
// baudrate
DICENTRY(BAUDRATE,      0x2003, 0, 1, 0)
// system control: 1- bootloader, 2 - save parameters, 3 - reset factory settings
DICENTRY(SYSCONTROL,    0x2007, 0, 1, 0)
// error state
DICENTRY(ERRSTATE,      0x6000, 0, 1, 0)
// controller status
DICENTRY(DEVSTATUS,     0x6001, 0, 1, 0)
// rotation direction
DICENTRY(ROTDIR,        0x6002, 0, 1, 0)
// maximal speed
DICENTRY(MAXSPEED,      0x6003, 0, 4, 1)
// relative displacement
DICENTRY(RELSTEPS,      0x6004, 0, 4, 0)
// operation mode
DICENTRY(OPMODE,        0x6005, 0, 1, 0)
// start speed
DICENTRY(STARTSPEED,    0x6006, 0, 2, 0)
// stop speed
DICENTRY(STOPSPEED,     0x6007, 0, 2, 0)
// acceleration coefficient
DICENTRY(ACCELCOEF,     0x6008, 0, 1, 0)
// deceleration coefficient
DICENTRY(DECELCOEF,     0x6009, 0, 1, 0)
// microstepping
DICENTRY(MICROSTEPS,    0x600A, 0, 2, 0)
// max current
DICENTRY(MAXCURNT,      0x600B, 0, 2, 0)
// current position
DICENTRY(POSITION,      0x600C, 0, 4, 0)
// motor enable
DICENTRY(ENABLE,        0x600E, 0, 1, 0)
// absolute displacement
DICENTRY(ABSSTEPS,      0x601C, 0, 4, 1)
// stop motor
DICENTRY(STOP,          0x6020, 0, 1, 0)

#define MAX_SPEED_MIN   -200000
#define MAX_SPEED_MAX   200000

// unclearable status
#define BUSY_STATE      (1<<3)
const char *devstatus(uint8_t status, uint8_t bit);
const char *errname(uint8_t error, uint8_t bit);
//int get_current_position(uint8_t NID);

#endif // PUSIROBOT_H__

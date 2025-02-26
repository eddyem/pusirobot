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
    const char *name;   // dictionary entry name
} SDO_dic_entry;

#ifndef DICENTRY
#define DICENTRY(name, idx, sidx, sz, s, n)  extern const SDO_dic_entry name;
#endif

#include "dicentries.in"

extern const int DEsz;
extern const SDO_dic_entry* allrecords[];

// speed limits
#define MAX_SPEED_MIN   -200000
#define MAX_SPEED_MAX   200000
// encoder ticks per rev * SPEED_MULTIPLIER = speed for 1 rev per second
#define SPEED_MULTIPLIER    (1.6)

// limit switches mask in GPIO status register (x=1,2,3)
#define EXTMASK(x)      (1<<(7+x))

#define EXTACTIVE(x, reg)   ((reg&EXTMASK(x)) ? 1:0)

// unclearable status
#define BUSY_STATE      (1<<3)
const char *devstatus(uint8_t status, uint8_t bit);
const char *errname(uint8_t error, uint8_t bit);
SDO_dic_entry *dictentry_search(uint16_t index, uint8_t subindex);
#endif // PUSIROBOT_H__

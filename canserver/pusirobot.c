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

#include <stdlib.h> // for NULL

#include "pusirobot.h"

// we should init constants here!
#undef DICENTRY
#define DICENTRY(name, idx, sidx, sz, s, n, v)  const SDO_dic_entry name = {idx, sidx, sz, s, n, v};
#include "dicentries.in"

// now init array with all dictionary
#undef DICENTRY
#define DICENTRY(name, idx, sidx, sz, s, n, v)  &name,
const SDO_dic_entry* allrecords[] = {
#include "dicentries.in"
};
const int DEsz = sizeof(allrecords) / sizeof(SDO_dic_entry*);

// controller status for bits
static const char *DevStatus[] = {
    "External stop 1",
    "External stop 2",
    "Stall state",
    "Busy state",
    "External stop 3",
    "The FIFO of PVT Mode 3 is empty",
    "FIFO Lower bound of PVT Mode 3",
    "FIFO upper limit of PVT mode 3"
};

// controller error statuses
static const char *DevError[] = {
    "TSD, over temperature shutdown",
    "AERR, coil A error",
    "BERR, coil B error",
    "AOC, A over current",
    "BOC, B over current",
    "UVLO, low voltage fault"
};

// return status message for given bit in status
const char *devstatus(uint8_t status, uint8_t bit){
    if(bit > 7) return NULL;
    if(status & (1<<bit)) return DevStatus[bit];
    return NULL;
}

// error codes explanation
const char *errname(uint8_t error, uint8_t bit){
    if(bit > 5) return NULL;
    if(error & (1<<bit)) return DevError[bit];
    return NULL;
}

// search if the object exists in dictionary - for config file parser
SDO_dic_entry *dictentry_search(uint16_t index, uint8_t subindex){
    // the search is linear as dictionary can be unsorted!!!
    for(int i = 0; i < DEsz; ++i){
        const SDO_dic_entry *entry = allrecords[i];
        if(entry->index == index && entry->subindex == subindex) return (SDO_dic_entry*)entry;
    }
    return NULL;
}

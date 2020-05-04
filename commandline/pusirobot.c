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

//#include <limits.h>

// we should init constants here!
#define DICENTRY(name, idx, sidx, sz, s)  const SDO_dic_entry name = {idx, sidx, sz, s};
#include "pusirobot.h"

/*
// get current position for node ID `NID`, @return INT_MIN if error
int get_current_position(uint8_t NID){
    int64_t val = SDO_read(&POSITION, NID);
    if(val == INT64_MIN) return INT_MIN;
    return (int) val;
}
*/


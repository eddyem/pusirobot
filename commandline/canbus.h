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
#ifndef CANBUS_H__
#define CANBUS_H__

#include <stdint.h>

#ifndef T_POLLING_TMOUT
#define T_POLLING_TMOUT (0.5)
#endif

typedef struct{
    uint32_t timemark;  // time since MCU run (ms)
    uint16_t ID;        // 11-bit identifier
    uint8_t data[8];    // up to 8 bit data, data[0] is lowest
    uint8_t len;        // data length
} CANmesg;

// main (necessary) functions of canbus.c:
void canbus_close();
int canbus_open(const char *devname);
int canbus_write(CANmesg *mesg);
int canbus_read(CANmesg *mesg);
int canbus_setspeed(int speed);

// auxiliary (not necessary) functions
void setserialspeed(int speed);
void showM(CANmesg *m);
CANmesg *parseCANmesg(const char *str);

#endif // CANBUS_H__

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
#ifndef CANOPEN_H__
#define CANOPEN_H__

#include "canbus.h"
#include "pusirobot.h"

// timeout for answer from the SDO, seconds
#define SDO_ANS_TIMEOUT     (1.)

// COB-ID base:
#define NMT_COBID           0
#define EMERG_COBID         0x80
#define TIMESTAMP_COBID     0x100
#define TPDO1_COBID         0x180
#define RPDO1_COBID         0x200
#define TPDO2_COBID         0x280
#define RPDO2_COBID         0x300
#define TPDO3_COBID         0x380
#define RPDO3_COBID         0x400
#define TPDO4_COBID         0x480
#define RPDO4_COBID         0x500
#define TSDO_COBID          0x580
#define RSDO_COBID          0x600
#define HEARTB_COBID        0x700
// mask to select COB-ID base from ID
#define COBID_MASK          0x780
// mask to select node ID from ID
#define NODEID_MASK         0x7F

// SDO client command specifier field
typedef enum{
    CCS_SEG_DOWNLOAD = 0,
    CCS_INIT_DOWNLOAD,
    CCS_INIT_UPLOAD,
    CCS_SEG_UPLOAD,
    CCS_ABORT_TRANSFER,
    CCS_BLOCK_UPLOAD,
    CCS_BLOCK_DOWNLOAD
} SDO_CCS_fields;
// make number from CSS field
#define SDO_CCS(f)  (f<<5)
// make CCS from number
#define GET_CCS(f)  (f>>5)
// make number from data amount
#define SDO_N(n)    ((4-n)<<2)
// make data amount from number
#define SDO_datalen(n) (4-((n&0xC)>>2))
// SDO e & s fields:
#define SDO_E       (1<<1)
#define SDO_S       (1<<0)

typedef struct{
    uint8_t NID;        // node ID in CANopen
    uint16_t index;     // SDO index
    uint8_t subindex;   // SDO subindex
    uint8_t data[4];    // 1..4 bytes of data (data[0] is lowest)
    uint8_t datalen;    // length of data
    uint8_t ccs;        // Client command specifier
} SDO;

const char *abortcode_text(uint32_t abortcode);
SDO *parseSDO(CANmesg *mesg);
SDO *readSDOvalue(uint16_t idx, uint8_t subidx, uint8_t NID);

int64_t SDO_read(const SDO_dic_entry *e, uint8_t NID);

int SDO_writeArr(const SDO_dic_entry *e, uint8_t NID, const uint8_t *data);
int SDO_write(const SDO_dic_entry *e, uint8_t NID, int64_t data);

//int SDO_readByte(uint16_t idx, uint8_t subidx, uint8_t *data, uint8_t NID);
#endif // CANOPEN_H__

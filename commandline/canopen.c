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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usefull_macros.h>

#include "canopen.h"

typedef struct{
    uint32_t code;
    const char *errmsg;
} abortcodes;


static const abortcodes AC[] = {
    //while read l; do N=$(echo $l|awk '{print $1 $2}'); R=$(echo $l|awk '{$1=$2=""; print substr($0,3)}'|sed 's/\.//'); echo -e "{0x$N, \"$R\"},"; done < codes.b
    {0x05030000, "Toggle bit not alternated"},
    {0x05040000, "SDO protocol timed out"},
    {0x05040001, "Client/server command specifier not valid or unknown"},
    {0x05040002, "Invalid block size (block mode only)"},
    {0x05040003, "Invalid sequence number (block mode only)"},
    {0x05040004, "CRC error (block mode only)"},
    {0x05040005, "Out of memory"},
    {0x06010000, "Unsupported access to an object"},
    {0x06010001, "Attempt to read a write only object"},
    {0x06010002, "Attempt to write a read only object"},
    {0x06020000, "Object does not exist in the object dictionary"},
    {0x06040041, "Object cannot be mapped to the PDO"},
    {0x06040042, "The number and length of the objects to be mapped would exceed PDO length"},
    {0x06040043, "General parameter incompatibility reason"},
    {0x06040047, "General internal incompatibility in the device"},
    {0x06060000, "Access failed due to a hardware error"},
    {0x06070010, "Data type does not match; length of service parameter does not match"},
    {0x06070012, "Data type does not match; length of service parameter too high"},
    {0x06070013, "Data type does not match; length of service parameter too low"},
    {0x06090011, "Sub-index does not exist"},
    {0x06090030, "Value range of parameter exceeded (only for write access)"},
    {0x06090031, "Value of parameter written too high"},
    {0x06090032, "Value of parameter written too low"},
    {0x06090036, "Maximum value is less than minimum value"},
    {0x08000000, "General error"},
    {0x08000020, "Data cannot be transferred or stored to the application"},
    {0x08000021, "Data cannot be transferred or stored to the application because of local control"},
    {0x08000022, "Data cannot be transferred or stored to the application because of the present device state"},
    {0x08000023, "Object dictionary dynamic generation fails or no object dictionary is present"},
};

static const int ACmax = sizeof(AC)/sizeof(abortcodes) - 1;

/**
 * @brief abortcode_text - explanation of abort code
 * @param abortcode - code
 * @return text for error or NULL
 */
const char *abortcode_text(uint32_t abortcode){ //, int *n){
    int idx = ACmax/2, min_ = 0, max_ = ACmax, newidx = 0, iter=0;
    do{
        ++iter;
        uint32_t c = AC[idx].code;
        //printf("idx=%d, min=%d, max=%d\n", idx, min_, max_);
        if(c == abortcode){
            //if(n) *n = iter;
            //DBG("got : %s", AC[idx].errmsg);
            return AC[idx].errmsg;
        }else if(c > abortcode){
            newidx = (idx + min_)/2;
            max_ = idx;

        }else{
            newidx = (idx + max_ + 1)/2;
            min_ = idx;
        }
        if(newidx == idx || min_ < 0 || max_ > ACmax){
            //if(n) *n = 0;
            return NULL;
        }
        idx = newidx;
    }while(1);
}

// make CAN message from sdo object; don't support more then one block/packet
static CANmesg *mkMesg(SDO *sdo){
    static CANmesg mesg;
    mesg.ID = RSDO_COBID + sdo->NID;
    mesg.len = 8;
    memset(mesg.data, 0, 8);
    mesg.data[0] = SDO_CCS(sdo->ccs);
    if(sdo->datalen){ // send N bytes of data
        mesg.data[0] |= SDO_N(sdo->datalen) | SDO_E | SDO_S;
        for(uint8_t i = 0; i < sdo->datalen; ++i) mesg.data[4+i] = sdo->data[i];
    }
    mesg.data[1] = sdo->index & 0xff; // l
    mesg.data[2] = (sdo->index >> 8) & 0xff; // h
    mesg.data[3] = sdo->subindex;
#if 0
    FNAME();
    green("Make message to 0x%X: ", mesg.ID);
    for(uint8_t i = 0; i < 8; ++i) printf("0x%02X ", mesg.data[i]);
    printf("\n");
#endif
    return &mesg;
}

// transform CAN-message to SDO
SDO *parseSDO(CANmesg *mesg){
    static SDO sdo;
    if(mesg->len != 8){
        WARNX("Wrong SDO data length");
        return NULL;
    }
    uint16_t cobid = mesg->ID & COBID_MASK;
    if(cobid != TSDO_COBID){
        DBG("cobid=0x%X, not a TSDO!", cobid);
        return NULL; // not a transmit SDO
    }
    sdo.NID = mesg->ID & NODEID_MASK;
    uint8_t spec = mesg->data[0];
    sdo.ccs = GET_CCS(spec);
    sdo.index = (uint16_t)mesg->data[1] | ((uint16_t)mesg->data[2] << 8);
    sdo.subindex = mesg->data[3];
    if((spec & SDO_E) && (spec & SDO_S)) sdo.datalen = SDO_datalen(spec);
    else if(sdo.ccs == CCS_ABORT_TRANSFER) sdo.datalen = 4; // error code
    else sdo.datalen = 0; // no data in message
    for(uint8_t i = 0; i < sdo.datalen; ++i) sdo.data[i] = mesg->data[4+i];
    DBG("Got TSDO from NID=%d, ccs=%u, index=0x%X, subindex=0x%X, datalen=%d", sdo.NID, sdo.ccs, sdo.index, sdo.subindex, sdo.datalen);
    return &sdo;
}

// send request to read SDO
static int ask2read(uint16_t idx, uint8_t subidx, uint8_t NID){
    SDO sdo;
    sdo.NID = NID;
    sdo.ccs = CCS_INIT_UPLOAD;
    sdo.datalen = 0;
    sdo.index = idx;
    sdo.subindex = subidx;
    CANmesg *mesg = mkMesg(&sdo);
    return canbus_write(mesg);
}

static SDO *getSDOans(uint16_t idx, uint8_t subidx, uint8_t NID){
    CANmesg mesg;
    SDO *sdo = NULL;
    double t0 = dtime();
    while(dtime() - t0 < SDO_ANS_TIMEOUT){
        mesg.ID = TSDO_COBID | NID; // read only from given ID
        if(canbus_read(&mesg)){
            continue;
        }
        sdo = parseSDO(&mesg);
        if(!sdo) continue;
        if(sdo->index == idx && sdo->subindex == subidx) break;
    }
    if(!sdo || sdo->index != idx || sdo->subindex != subidx){
        WARNX("No answer from SDO 0x%X/0x%X", idx, subidx);
        return NULL;
    }
    return sdo;
}

/**
 * @brief readSDOvalue - send request to SDO read
 * @param idx    - SDO index
 * @param subidx - SDO subindex
 * @param NID    - target node ID
 * @return SDO received or NULL if error
 */
SDO *readSDOvalue(uint16_t idx, uint8_t subidx, uint8_t NID){
    if(ask2read(idx, subidx, NID)){
        WARNX("Can't initiate upload");
        return NULL;
    }
    return getSDOans(idx, subidx, NID);
}

static inline uint32_t mku32(uint8_t data[4]){
    return (uint32_t)(data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
}

static inline uint16_t mku16(uint8_t data[4]){
    return (uint16_t)(data[0] | (data[1]<<8));
}

static inline uint8_t mku8(uint8_t data[4]){
    return data[0];
}

static inline int32_t mki32(uint8_t data[4]){
    return (int32_t)(data[0] | (data[1]<<8) | (data[2]<<16) | (data[3]<<24));
}

static inline int16_t mki16(uint8_t data[4]){
    return (int16_t)(data[0] | (data[1]<<8));
}

static inline int8_t mki8(uint8_t data[4]){
    return (int8_t)data[0];
}

// read SDO value, if error - return INT64_MIN
int64_t SDO_read(const SDO_dic_entry *e, uint8_t NID){
    SDO *sdo = readSDOvalue(e->index, e->subindex, NID);
    if(!sdo){
        WARNX("SDO read error");
        return INT64_MIN;
    }
    if(sdo->ccs == CCS_ABORT_TRANSFER){ // error
        WARNX("Got error for SDO 0x%X", e->index);
        uint32_t ac = mku32(sdo->data);
        const char *etxt = abortcode_text(ac);
        if(etxt) red("Abort code 0x%X: ", ac, etxt);
        return INT64_MIN;
    }
    if(sdo->datalen != e->datasize){
        WARNX("Got SDO with length %d instead of %d (as in dictionary)", sdo->datalen, e->datasize);
    }
    int64_t ans = 0;
    if(e->issigned){
        switch(sdo->datalen){
            case 1:
                ans = mki8(sdo->data);
            break;
            case 4:
                ans = mki32(sdo->data);
            break;
            default: // can't be 3! 3->2
                ans = mki16(sdo->data);
        }
    }else{
        switch(sdo->datalen){
            case 1:
                ans = mku8(sdo->data);
            break;
            case 4:
                ans = mku32(sdo->data);
            break;
            default: // can't be 3! 3->2
                ans = mku16(sdo->data);
        }
    }
    return ans;
}

// write SDO data, return 0 if all OK
int SDO_writeArr(const SDO_dic_entry *e, uint8_t NID, const uint8_t *data){
    if(!e || !data || e->datasize < 1 || e->datasize > 4){
        WARNX("SDO_write(): bad datalen");
        return 1;
    }
    SDO sdo;
    sdo.NID = NID;
    sdo.ccs = CCS_INIT_DOWNLOAD;
    sdo.datalen = e->datasize;
    for(uint8_t i = 0; i < e->datasize; ++i) sdo.data[i] = data[i];
    sdo.index = e->index;
    sdo.subindex = e->subindex;
    CANmesg *mesgp = mkMesg(&sdo);
    if(canbus_write(mesgp)){
        WARNX("SDO_write(): Can't initiate download");
        return 2;
    }
    SDO *sdop = getSDOans(e->index, e->subindex, NID);
    if(!sdop){
        WARNX("SDO_write(): SDO read error");
        return 3;
    }
    if(sdop->ccs == CCS_ABORT_TRANSFER){ // error
        WARNX("SDO_write(): Got error for SDO 0x%X", e->index);
        uint32_t ac = mku32(sdop->data);
        const char *etxt = abortcode_text(ac);
        if(etxt) WARNX("Abort code 0x%X: %s", ac, etxt);
        return 4;
    }
    if(sdop->datalen != 0){
        WARNX("SDO_write(): got answer with non-zero length");
        return 5;
    }
    if(sdop->ccs != CCS_SEG_UPLOAD){
        WARNX("SDO_write(): got wrong answer");
        return 6;
    }
    return 0;
}

int SDO_write(const SDO_dic_entry *e, uint8_t NID, int64_t data){
    if(!e) return 1;
    uint8_t arr[4] = {0};
    uint32_t U;
    int32_t I;
    uint16_t U16;
    int16_t I16;
    if(e->issigned){
        switch(e->datasize){
            case 1:
                arr[0] = (uint8_t) data;
            break;
            case 4:
                I = (int32_t) data;
                arr[0] = I&0xff;
                arr[1] = (I>>8)&0xff;
                arr[2] = (I>>16)&0xff;
                arr[3] = (I>>24)&0xff;
            break;
            default: // can't be 3! 3->2
                I16 = (int16_t) data;
                arr[0] = I16&0xff;
                arr[1] = (I16>>8)&0xff;
        }
    }else{
        switch(e->datasize){
            case 1:
                arr[0] = (uint8_t) data;
            break;
            case 4:
                U = (uint32_t) data;
                arr[0] = U&0xff;
                arr[1] = (U>>8)&0xff;
                arr[2] = (U>>16)&0xff;
                arr[3] = (U>>24)&0xff;
            break;
            default: // can't be 3! 3->2
                U16 = (uint16_t) data;
                arr[0] = U16&0xff;
                arr[1] = (U16>>8)&0xff;
        }
    }
    /*
    DBG("DATA:");
    for(int i = 0; i < e->datasize; ++i) printf("0x%X ", arr[i]);
    printf("\n");
    return 0;*/
    return SDO_writeArr(e, NID, arr);
}



// read one byte of data
/*int SDO_readByte(uint16_t idx, uint8_t subidx, uint8_t *data, uint8_t NID){
    SDO *sdo = readSDOvalue(idx, subidx, NID);
    if(!sdo || sdo->datalen != 1){
        WARNX("Got SDO with wrong data length: %d instead of 1", sdo->datalen);
        return 1;
    }
    if(data) *data = sdo->data[0];
    return 0;
}*/

#if 0
// write uint8_t to SDO with index idx & subindex subidx to NID
void SDO_writeU8(uint16_t idx, uint8_t subidx, uint8_t data, uint8_t NID){
    SDO sdo;
    sdo.NID = NID;
    sdo.ccs = CCS_INIT_DOWNLOAD;
}
#endif

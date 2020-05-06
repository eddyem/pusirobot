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

#include "canopen.h"
#include "dataparser.h"
#include "pusirobot.h"

#include <libgen.h> // basename
#include <stdio.h>  // fopen
#include <string.h> // strchr
#include <usefull_macros.h>

#define BUFSZ 256

char *getl(char *str, long long *lp){
    long long l;
    char *eptr;
    int base = 0;
    if(str[0] == '0' && (str[1] == 'b' || str[1] == 'B')){ // binary number format
        base = 2;
        str += 2;
    }
    l = strtoll(str, &eptr, base);
    if(str == eptr) return NULL;
    if(lp) *lp = l;
    return eptr;
}

// get next value - after ',' and spaces
char *getnxt(char *str){
    char *comma = strchr(str, ',');
    if(!comma || !*comma) return NULL;
    for(; str < comma; ++str) if(*str > ' ') return NULL;
    ++comma;
    while(*comma && *comma <= ' ') ++comma;
    return comma;
}

/**
 * @brief parse_data_file - read SDO & data from file and send them to node `nid`
 * @param fname - file name
 * @param nid   - node ID or 0 to check file
 * @return 0 if no errors found
 */
int parse_data_file(const char *fname, uint8_t nid){
    int ret = 0;
    if(!fname){
        WARNX("No filename for parsing");
        return 1;
    }
    FILE *f = fopen(fname, "r");
    if(!f){
        WARN("Can't open %s for parsing", basename((char*)fname));
        return 2;
    }
    char str[BUFSZ];
    int lineno = 0;
    while(fgets(str, BUFSZ, f)){
        char *num = strchr(str, '#');
        if(num) *num = 0;
        long long l;
        uint16_t idx;
        uint8_t sidx;
        int64_t data;
        int isgood = 1;
        do{
            char *ptr;
            if(!(ptr = getl(str, &l))) break;
            idx = (uint16_t) l;
            if(!(ptr = getnxt(ptr))){
                isgood = 0;
                break;
            }
            if(!(ptr = getl(ptr, &l))){
                isgood = 0;
                break;
            }
            sidx = (uint8_t) l;
            if(!(ptr = getnxt(ptr))){
                isgood = 0;
                break;
            }
            if(!(ptr = getl(ptr, &l))){
                isgood = 0;
                break;
            }
            data = (int64_t) l;
            DBG("Got: idx=0x%04X, subidx=0x%02X, data=0x%lX", idx, sidx, data);
            if(nid == 0) printf("line #%d: read SDO with index=0x%04X, subindex=0x%02X, data=0x%lX (dec: %ld)\n", lineno, idx, sidx, data, data);
            SDO_dic_entry *entry = dictentry_search(idx, sidx);
            if(!entry){
                WARNX("SDO 0x%04X/0x%02X isn't in dictionary", idx, sidx);
                continue;
            }
            if(data<0 && !entry->issigned){
                WARNX("SDO 0x%04X/0x%02X is only positive", idx, sidx);
                continue;
            }
            uint64_t u64 = (uint64_t) (l > 0) ? l : -l;
            if(u64 > UINT32_MAX){
                WARNX("The data size of SDO 0x%04X/0x%02X out of possible range for uint32_t", idx, sidx);
                continue;
            }
            uint32_t u = (uint32_t) (l > 0) ? l : -l;
            if(entry->datasize != 4){
                uint32_t mask = 0xff;
                if(entry->datasize == 2) mask = 0xffff;
                if((u & mask) != u){
                    WARNX("The data size of SDO 0x%04X/0x%02X is larger than possible (%d byte[s])", idx, sidx, entry->datasize);
                    continue;
                }
            }
            if(nid) SDO_write(entry, nid, data);
        }while(0);
        if(!isgood){
            WARNX("Bad syntax in line #%d: %s\nFormat: index, subindex, data (all may be hex, dec, oct or bin)", lineno, str);
        }
        ++lineno;
    }
    fclose(f);
    return ret;
}


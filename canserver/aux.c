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

// auxiliary functions

#include "aux.h"

#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usefull_macros.h>

static char *VID = NULL, *PID = NULL;
// try to find serial device with given vid/pid/filepath
// @return device filepath (allocated here!) or NULL if not found
char *find_device(){
    char *path = NULL;
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    udev = udev_new();
    if(!udev){
        WARN("Can't create udev");
        return NULL;
    }
    // Create a list of the devices in the 'hidraw' subsystem.
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    // Check out each device found
    udev_list_entry_foreach(dev_list_entry, devices){
        struct udev_device *dev;
        const char *lpath = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, lpath);
        const char *devpath = udev_device_get_devnode(dev);
        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
        if(!dev){
            udev_device_unref(dev);
            continue;
        }
        //const char *pardevpath = udev_device_get_devnode(dev);
        const char *vid, *pid;
        vid = udev_device_get_sysattr_value(dev,"idVendor");
        pid = udev_device_get_sysattr_value(dev, "idProduct");
        DBG("Next device: VID/PID= %s/%s; path=%s; devpath=%s\n", vid, pid, lpath, devpath);
        if(!devpath){
            udev_device_unref(dev);
            continue;
        }
        int found = 0;
        do{
            // 1st try to check recently found VID/PID
            int good = 0;
            if(VID){ if(strcmp(VID, vid)) break; else ++good;}
            if(PID){ if(strcmp(PID, pid)) break; else ++good;}
            if(good == 2){
                DBG("Use recently found VID/PID");
                found = 1; break;
            }
            // user give us VID and/or PID? Check them
            if(GP->vid){ if(strcmp(GP->vid, vid)) break;} // user give another vid
            if(GP->pid){ if(strcmp(GP->vid, vid)) break;} // user give another pid
            // now try to check by user given device name
            if(GP->device){
                if(strcmp(GP->device, devpath) == 0) found = 1;
                break;
            }
            // still didn't found? OK, take the first comer!
            DBG("The first comer");
            found = 1;
        }while(0);
        if(found){
            FREE(VID); FREE(PID);
            VID = strdup(vid); PID = strdup(pid);
            path = strdup(devpath);
            DBG("Found: VID=%s, PID=%s, path=%s", VID, PID, path);
        }
        /*printf("\tman: %s, prod: %s\n", udev_device_get_sysattr_value(dev,"manufacturer"),
               udev_device_get_sysattr_value(dev,"product"));
        printf("\tparent: %s\n", pardevpath);*/
        udev_device_unref(dev);
        if(found) break;
    }
    // Free the enumerator object
    udev_enumerate_unref(enumerate);
    return path;
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

/**
 * @brief str2long - convert full string to long
 * @param str   - string
 * @param l (o) - converted number
 * @return 0 if OK, 1 if failed
 */
int str2long(char *str, long* l){
    if(!str) return 1;
    char *eptr = NULL;
    long n = strtol(str, &eptr, 0);
    if(*eptr) return 2; // wrong symbols in number
    if(l) *l = n;
    //DBG("str '%s' to long %ld", str, n);
    return 0;
}

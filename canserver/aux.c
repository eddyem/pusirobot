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

#include <fcntl.h>
#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usefull_macros.h>

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
        int found = FALSE;
        do{
            // user give us VID and/or PID? Check them
            if(GP->vid){ if(strcmp(GP->vid, vid)) break;} // user give another vid
            if(GP->pid){ if(strcmp(GP->vid, vid)) break;} // user give another pid
            // now try to check by user given device name
            if(GP->device){
                DBG("User gives device name: %s", GP->device);
                if(strcmp(GP->device, devpath) == 0) found = TRUE;
                break;
            }
            // still didn't found? OK, take the first comer!
            DBG("The first comer");
            found = TRUE;
        }while(0);
        if(found){
            if(open(devpath, O_RDWR) < 0){
                WARN("open()");
                found = FALSE;
            }else{
                path = strdup(devpath);
                DBG("Found: VID=%s, PID=%s, path=%s", vid, pid, path);
            }
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

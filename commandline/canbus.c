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

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <usefull_macros.h>

#include "canbus.h"

#ifndef BUFLEN
#define BUFLEN 80
#endif

#ifndef WAIT_TMOUT
#define WAIT_TMOUT 0.01
#endif

/*
This file should provide next functions:
  int canbus_open(const char *devname) - calls @the beginning, return 0 if all OK
  int canbus_setspeed(int speed) - set given speed (in Kbaud) @ CAN bus (return 0 if all OK)
  void canbus_close() - calls @the end
  int canbus_write(CANmesg *mesg) - write `data` with length `len` to ID `ID`, return 0 if all OK
  int canbus_read(CANmesg *mesg) - blocking read (broadcast if ID==0 or only from given ID) from can bus, return 0 if all OK
*/

static sl_tty_t *dev = NULL;  // shoul be global to restore if die
static int serialspeed = 115200; // speed to open serial device
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static char *read_string();

/**
 * @brief read_ttyX- read data from TTY with 10ms timeout WITH disconnect detection
 * @param buff (o) - buffer for data read
 * @param length   - buffer len
 * @return amount of bytes read
 */
static int read_ttyX(sl_tty_t *d){
    if(!d || d->comfd < 0) return -1;
    size_t L = 0;
    ssize_t l;
    size_t length = d->bufsz;
    char *ptr = d->buf;
    fd_set rfds;
    struct timeval tv;
    int retval;
    do{
        l = 0;
        FD_ZERO(&rfds);
        FD_SET(d->comfd, &rfds);
        tv.tv_sec = 0; tv.tv_usec = 500;
        retval = select(d->comfd + 1, &rfds, NULL, NULL, &tv);
        if (!retval) break;
        if(FD_ISSET(d->comfd, &rfds)){
            if((l = read(d->comfd, ptr, length)) < 1){
                return -1; // disconnect or other error - close TTY & die
            }
            ptr += l; L += l;
            length -= l;
        }
    }while(l && length);
    d->buflen = L;
    d->buf[L] = 0;
    return (size_t)L;
}


// thread-safe writing, add trailing '\n'
static int ttyWR(const char *buff, int len){
    FNAME();
    pthread_mutex_lock(&mutex);
    //canbus_clear();
    read_string(); // clear RX buffer
    DBG("Write 2tty %d bytes: ", len);
#ifdef EBUG
    int _U_ n = write(STDERR_FILENO, buff, len);
    fprintf(stderr, "\n");
    double t0 = sl_dtime();
#endif
    int w = sl_tty_write(dev->comfd, buff, (size_t)len);
    if(!w) w = sl_tty_write(dev->comfd, "\n", 1);
    DBG("Written, dt=%g", sl_dtime() - t0);
    /*
    int errctr = 0;
    while(1){
        char *s = read_string(); // clear echo & check
        if(!s || strncmp(s, buff, strlen(buff)) != 0){
            if(++errctr > 3){
                WARNX("wrong answer! Got '%s' instead of '%s'", s, buff);
                w = 1;
                break;
            }
        }else break;
    }*/
    pthread_mutex_unlock(&mutex);
    DBG("Success, dt=%g", sl_dtime() - t0);
    return w;
}

void canbus_close(){
    if(dev) sl_tty_close(&dev);
}

void setserialspeed(int speed){
    serialspeed = speed;
}

void canbus_clear(){
    while(read_ttyX(dev));
}

int canbus_open(const char *devname){
    if(!devname){
        WARNX("canbus_open(): need device name");
        return 1;
    }
    if(dev) sl_tty_close(&dev);
    dev = sl_tty_new((char*)devname, serialspeed, BUFLEN);
    if(dev){
        if(!sl_tty_open(dev, 1)) // blocking open
            sl_tty_close(&dev);
    }
    if(!dev){
        return 1;
    }
    return 0;
}

int canbus_setspeed(int speed){
    if(speed == 0) return 0; // default - not change
    char buff[BUFLEN];
    if(speed < 10 || speed > 3000){
        WARNX("Wrong CAN bus speed value: %d", speed);
        return 1;
    }
    int len = snprintf(buff, BUFLEN, "b %d", speed);
    if(len < 1) return 2;
    int r = ttyWR(buff, len);
    read_string(); // clear RX buf ('Reinit CAN bus with speed XXXXkbps')
    return r;
}

int canbus_write(CANmesg *mesg){
    FNAME();
    char buf[BUFLEN];
    if(!mesg || mesg->len > 8) return 1;
    int rem = BUFLEN, len = 0;
    int l = snprintf(buf, rem, "s %d", mesg->ID);
    rem -= l; len += l;
    for(uint8_t i = 0; i < mesg->len; ++i){
        l = snprintf(&buf[len], rem, " %d", mesg->data[i]);
        rem -= l; len += l;
        if(rem < 0) return 2;
    }
    canbus_clear();
    return ttyWR(buf, len);
}

/**
 * read strings from terminal (ending with '\n') with timeout
 * @return NULL if nothing was read or pointer to static buffer
 */
static char *read_string(){
    static char buf[1024];
    int LL = 1023, r = 0, l;
    char *ptr = NULL;
    static char *optr = NULL;
    if(optr && *optr){
        ptr = optr;
        optr = strchr(optr, '\n');
        if(optr){
            *optr = 0;
            ++optr;
        }
        return ptr;
    }
    ptr = buf;
    double d0 = sl_dtime();
    do{
        if((l = read_ttyX(dev))){
            if(l < 0){
                ERR("tty disconnected");
            }
            if(l > LL){ // buffer overflow
                WARNX("read_string(): buffer overflow");
                optr = NULL;
                return NULL;
            }
            memcpy(ptr, dev->buf, dev->buflen);
            r += l; LL -= l; ptr += l;
            if(ptr[-1] == '\n'){
                DBG("Newline detected");
                break;
            }
            d0 = sl_dtime();
        }
    }while(sl_dtime() - d0 < WAIT_TMOUT && LL);
    if(r){
        buf[r] = 0;
        optr = strchr(buf, '\n');
        if(optr){
            *optr = 0;
            ++optr;
            if(*optr) DBG("optr=%s", optr);
        }else{
            WARNX("read_string(): no newline found");
            DBG("buf: %s", buf);
            optr = NULL;
            return NULL;
        }
        DBG("buf: %s, time: %g", buf, sl_dtime() - d0);
        return buf;
    }
    return NULL;
}

CANmesg *parseCANmesg(const char *str){
    static CANmesg m;
    int l = sscanf(str, "%d #0x%hx 0x%hhx 0x%hhx 0x%hhx 0x%hhx 0x%hhx 0x%hhx 0x%hhx 0x%hhx", &m.timemark, &m.ID,
                   &m.data[0], &m.data[1], &m.data[2], &m.data[3], &m.data[4], &m.data[5], &m.data[6], &m.data[7]);
    if(l < 2) return NULL;
    m.len = l - 2;
    return &m;
}

#ifdef EBUG
void showM(CANmesg *m){
    printf("TS=%d, ID=0x%X", m->timemark, m->ID);
    int l = m->len;
    if(l) printf(", data=");
    for(int i = 0; i < l; ++i) printf(" 0x%02X", m->data[i]);
    printf("\n");
}
#endif

int canbus_read(CANmesg *mesg){
    if(!mesg) return 1;
    pthread_mutex_lock(&mutex);
    double t0 = sl_dtime();
    int ID = mesg->ID;
    char *ans;
    CANmesg *m;
    while(sl_dtime() - t0 < T_POLLING_TMOUT){ // read answer
        if((ans = read_string())){ // parse new data
            if((m = parseCANmesg(ans))){
                DBG("Got canbus message (dT=%g):", sl_dtime() - t0);
#ifdef EBUG
                showM(m);
#endif
                if(ID && m->ID == ID){
                    memcpy(mesg, m, sizeof(CANmesg));
                    DBG("All OK");
                    pthread_mutex_unlock(&mutex);
                    return 0;
                }
            }
        }
    }
    pthread_mutex_unlock(&mutex);
    return 1;
}


/*
 * This file is part of the stepper project.
 * Copyright 2022 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

#ifdef EBUG
#define MAXVERBLVL  1
#define LOGLVL      LOGLEVEL_DBG
#else
#define MAXVERBLVL  0
#define LOGLVL      LOGLEVEL_WARN
#endif

#define LogAndWarn(...)     do{LOGWARN(__VA_ARGS__); WARNX(__VA_ARGS__);}while(0)
#define LogAndErr(...)      do{LOGERR(__VA_ARGS__); ERRX(__VA_ARGS__);}while(0)
void message(int level, const char *fmt, ...);
void maxmesglevl(int level);

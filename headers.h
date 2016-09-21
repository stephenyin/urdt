/*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#ifndef __HEADERS_H__
#define __HEADERS_H__


/* standard C headers */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define HAVE_POSIX_THREAD
#define CONCAT(a,c) a c

#ifdef HAVE_STRINGS_H
    #include <strings.h>
#endif // HAVE_STRINGS_H

/* unix headers */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>
#include <syslog.h>

#ifdef HAVE_POSIX_THREAD
    #include <pthread.h>
#endif // HAVE_POSIX_THREAD

#define MAX_PATH    255

#include "vlog.h"

#endif /* __HEADERS_H__ */


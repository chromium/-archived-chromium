// Copyright 2008 Google Inc.
// Author: Lincoln Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// A config.h to be used for MS Visual Studio projects, when the configure
// script cannot be used to automatically determine the capabilities of
// the environment.

#ifndef OPEN_VCDIFF_VSPROJECTS_CONFIG_H_
#define OPEN_VCDIFF_VSPROJECTS_CONFIG_H_

/* Define to 1 if you have the `InitializeCriticalSection' function. */
#define HAVE_INITIALIZECRITICALSECTION 1

/* Define to 1 if you have the `InterlockedCompareExchange' function. */
#define HAVE_INTERLOCKEDCOMPAREEXCHANGE 1

/* Define to 1 if you have the `QueryPerformanceCounter' function. */
#define HAVE_QUERYPERFORMANCECOUNTER 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if the system has the type `__int16'. */
#define HAVE___INT16 1


/* Name of package */
#define PACKAGE "open-vcdiff"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "opensource@google.com"

/* Define to the full name of this package. */
#define PACKAGE_NAME "open-vcdiff"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "open-vcdiff 0.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "open-vcdiff"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.1"

// These functions have different names, but the same behavior,
// for Visual Studio.
#define strcasecmp _stricmp
#define snprintf _snprintf

#endif  // OPEN_VCDIFF_VSPROJECTS_CONFIG_H_

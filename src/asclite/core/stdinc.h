/*
 * asclite
 * Author: Jerome Ajot, Nicolas Radde, Chris Laprun
 *
 * This software was developed at the National Institute of Standards and Technology by
 * employees of the Federal Government in the course of their official duties.  Pursuant to
 * Title 17 Section 105 of the United States Code this software is not subject to copyright
 * protection within the United States and is in the public domain. asclite is
 * an experimental system.  NIST assumes no responsibility whatsoever for its use by any party.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS."  With regard to this software, NIST MAKES NO EXPRESS
 * OR IMPLIED WARRANTY AS TO ANY MATTER WHATSOEVER, INCLUDING MERCHANTABILITY,
 * OR FITNESS FOR A PARTICULAR PURPOSE.
 */
	
#ifndef STDINC_H
#define STDINC_H

#include <string.h>
#include <iostream>
#include <ostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <list>
#include <vector>
#include <set>
#include <assert.h>
#include <sstream>
#include <float.h>
#include <limits.h>

#include "minilzo.h"

#ifdef __MACH__
extern "C" {
#include <mach/mach.h>
}
#endif

using namespace std;

typedef unsigned int uint;
typedef unsigned long int ulint;
typedef unsigned long ulong;
typedef unsigned long long int ullint;

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 255
#endif

#ifndef INT_MAX2
#define INT_MAX2 INT_MAX/2
#endif

#ifndef M_LOG2E
#define M_LOG2E (double) 1.4426950408889634074
#endif

#ifndef F_ROUND
#define F_ROUND(_n,_p)    (((double)((int)((_n) * pow(10.0,(double)(_p)) + ((_n>0.0)?0.5:-0.5)))) / pow(10.0,(double)(_p)))
#endif

#endif

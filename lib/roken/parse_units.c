/*
 * Copyright (c) 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      H�gskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id$");
#endif

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <roken.h>
#include "parse_units.h"

/*
 * Parse string in `s' according to `units' and return value.
 * def_unit defines the default unit.
 */

static int
parse_something (const char *s, const struct units *units,
		 const char *def_unit,
		 int (*func)(int res, int val, unsigned mult),
		 int init,
		 int accept_no_val_p)
{
    const char *p;
    int res = init;
    unsigned def_mult = 1;

    if (def_unit != NULL) {
	const struct units *u;

	for (u = units; u->name; ++u) {
	    if (strcasecmp (u->name, def_unit) == 0) {
		def_mult = u->mult;
		break;
	    }
	}
	if (u->name == NULL)
	    return -1;
    }

    p = s;
    while (*p) {
	double val;
	char *next;
	const struct units *u, *partial_unit;
	size_t u_len;
	unsigned partial;

	val = strtod (p, &next); /* strtol(p, &next, 0); */
	if (val == 0 && p == next)
	    if(accept_no_val_p)
		val = 1;
	    else
		return -1;
	p = next;
	while (isspace(*p))
	    ++p;
	if (*p == '\0') {
	    res = (*func)(res, val, def_mult);
	    if (res < 0)
		return res;
	    break;
	} else if (*p == '+') {
	    ++p;
	} else if (*p == '-') {
	    ++p;
	    val = -1;
	}
	u_len = strcspn (p, "0123456789 \t");
	partial = 0;
	partial_unit = NULL;
	if (u_len > 1 && p[u_len - 1] == 's')
	    --u_len;
	for (u = units; u->name; ++u) {
	    if (strncasecmp (p, u->name, u_len) == 0) {
		if (u_len == strlen (u->name)) {
		    p += u_len;
		    res = (*func)(res, val, u->mult);
		    if (res < 0)
			return res;
		    break;
		} else {
		    ++partial;
		    partial_unit = u;
		}
	    }
	}
	if (u->name == NULL)
	    if (partial == 1) {
		p += u_len;
		res = (*func)(res, val, partial_unit->mult);
		if (res < 0)
		    return res;
	    } else {
		return -1;
	    }
	if (*p == 's')
	    ++p;
    }
    return res;
}

/*
 * The string consists of a sequence of `n unit'
 */

static int
acc_units(int res, int val, unsigned mult)
{
    return res + val * mult;
}

int
parse_units (const char *s, const struct units *units,
	     const char *def_unit)
{
    return parse_something (s, units, def_unit, acc_units, 0, 0);
}

/*
 * The string consists of a sequence of `[+-]flag'.  `orig' consists
 * the original set of flags, those are then modified and returned as
 * the function value.
 */

static int
acc_flags(int res, int val, unsigned mult)
{
    if(val == 1)
	return res | mult;
    else if(val == -1)
	return res & ~mult;
    else
	return -1;
}

int
parse_flags (const char *s, const struct units *units,
	     int orig)
{
    return parse_something (s, units, NULL, acc_flags, orig, 1);
}

/*
 * Return a string representation according to `units' of `num' in `s'
 * with maximum length `len'.  The actual length is the function value.
 */

static size_t
unparse_something (int num, const struct units *units, char *s, size_t len,
		   int (*print) (char *s, size_t len, int div,
				const char *name, int rem),
		   int (*update) (int in, unsigned mult),
		   const char *zero_string)
{
    const struct units *u;
    size_t ret = 0, tmp;

    if (num == 0)
	return snprintf (s, len, "%s", zero_string);

    for (u = units; num > 0 && u->name; ++u) {
	int div;

	div = num / u->mult;
	if (div) {
	    num = (*update) (num, u->mult);
	    tmp = (*print) (s, len, div, u->name, num);

	    len -= tmp;
	    s += tmp;
	    ret += tmp;
	}
    }
    return ret;
}

static int
print_unit (char *s, size_t len, int div, const char *name, int rem)
{
    return snprintf (s, len, "%u %s%s%s",
		     div, name,
		     div == 1 ? "" : "s",
		     rem > 0 ? " " : "");
}

static int
update_unit (int in, unsigned mult)
{
    return in % mult;
}

size_t
unparse_units (int num, const struct units *units, char *s, size_t len)
{
    return unparse_something (num, units, s, len,
			      print_unit,
			      update_unit,
			      "0");
}

static int
print_flag (char *s, size_t len, int div, const char *name, int rem)
{
    return snprintf (s, len, "%s%s", name, rem > 0 ? ", " : "");
}

static int
update_flag (int in, unsigned mult)
{
    return in - mult;
}

size_t
unparse_flags (int num, const struct units *units, char *s, size_t len)
{
    return unparse_something (num, units, s, len,
			      print_flag,
			      update_flag,
			      "");
}

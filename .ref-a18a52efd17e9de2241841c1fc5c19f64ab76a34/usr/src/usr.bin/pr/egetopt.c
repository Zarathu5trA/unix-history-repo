/*-
 * Copyright (c) 1991 Keith Muller.
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Keith Muller of the University of California, San Diego.
 *
 * %sccs.include.redist.c%
 */

#ifndef lint
static char sccsid[] = "@(#)egetopt.c	8.1 (Berkeley) %G%";
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

/*
 * egetopt:	get option letter from argument vector (an extended
 *		version of getopt).
 *
 * Non standard additions to the ostr specs are:
 * 1) '?': immediate value following arg is optional (no white space
 *    between the arg and the value)
 * 2) '#': +/- followed by a number (with an optional sign but
 *    no white space between the arg and the number). The - may be
 *    combined with other options, but the + cannot.
 */

int	eopterr = 1;		/* if error message should be printed */
int	eoptind = 1;		/* index into parent argv vector */
int	eoptopt;		/* character checked for validity */
char	*eoptarg;		/* argument associated with option */

#define	BADCH	(int)'?'
#define	EMSG	""

int
egetopt(nargc, nargv, ostr)
	int nargc;
	char * const *nargv;
	const char *ostr;
{
	static char *place = EMSG;	/* option letter processing */
	register char *oli;		/* option letter list index */
	static int delim;		/* which option delimeter */
	register char *p;
	static char savec = '\0';

	if (savec != '\0') {
		*place = savec;
		savec = '\0';
	}

	if (!*place) {
		/*
		 * update scanning pointer
		 */
		if ((eoptind >= nargc) ||
		    ((*(place = nargv[eoptind]) != '-') && (*place != '+'))) {
			place = EMSG;
			return (EOF);
		}

		delim = (int)*place;
		if (place[1] && *++place == '-' && !place[1]) {
			/*
			 * found "--"
			 */
			++eoptind;
			place = EMSG;
			return (EOF);
		}
	}

	/*
	 * check option letter
	 */
	if ((eoptopt = (int)*place++) == (int)':' || (eoptopt == (int)'?') ||
	    !(oli = strchr(ostr, eoptopt))) {
		/*
		 * if the user didn't specify '-' as an option,
		 * assume it means EOF when by itself.
		 */
		if ((eoptopt == (int)'-') && !*place)
			return (EOF);
		if (strchr(ostr, '#') && (isdigit(eoptopt) ||
		    (((eoptopt == (int)'-') || (eoptopt == (int)'+')) &&
		      isdigit(*place)))) {
			/*
			 * # option: +/- with a number is ok
			 */
			for (p = place; *p != '\0'; ++p) {
				if (!isdigit(*p))
					break;
			}
			eoptarg = place-1;

			if (*p == '\0') {
				place = EMSG;
				++eoptind;
			} else {
				place = p;
				savec = *p;
				*place = '\0';
			}
			return (delim);
		}

		if (!*place)
			++eoptind;
		if (eopterr) {
			if (!(p = strrchr(*nargv, '/')))
				p = *nargv;
			else
				++p;
			(void)fprintf(stderr, "%s: illegal option -- %c\n",
			    p, eoptopt);
		}
		return (BADCH);
	}
	if (delim == (int)'+') {
		/*
		 * '+' is only allowed with numbers
		 */
		if (!*place)
			++eoptind;
		if (eopterr) {
			if (!(p = strrchr(*nargv, '/')))
				p = *nargv;
			else
				++p;
			(void)fprintf(stderr,
				"%s: illegal '+' delimiter with option -- %c\n",
				p, eoptopt);
		}
		return (BADCH);
	}
	++oli;
	if ((*oli != ':') && (*oli != '?')) {
		/*
		 * don't need argument
		 */
		eoptarg = NULL;
		if (!*place)
			++eoptind;
		return (eoptopt);
	}

	if (*place) {
		/*
		 * no white space
		 */
		eoptarg = place;
	} else if (*oli == '?') {
		/*
		 * no arg, but NOT required
		 */
		eoptarg = NULL;
	} else if (nargc <= ++eoptind) {
		/*
		 * no arg, but IS required
		 */
		place = EMSG;
		if (eopterr) {
			if (!(p = strrchr(*nargv, '/')))
				p = *nargv;
			else
				++p;
			(void)fprintf(stderr,
			    "%s: option requires an argument -- %c\n", p,
			    eoptopt);
		}
		return (BADCH);
	} else {
		/*
		 * arg has white space
		 */
		eoptarg = nargv[eoptind];
	}
	place = EMSG;
	++eoptind;
	return (eoptopt);
}

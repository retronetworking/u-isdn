/* intime.c
   Parse a time string into a uuconf_timespan structure.

   Modified by Georgios Papoutsis on Sun Oct 29 1995 for use with
   Matthias Urlichs' ISDN package.

   Further modified (read: heavily stripped) by Matthias Urlichs.

   Copyright (C) 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, Building 200, 1 Kendall Square, Cambridge, MA 02139.
   */

#include "master.h"

static const struct
{
  const char *zname;
  int imin;
  int imax;
} asTdays[] =
{
  { "any", 0, 6 },
  { "wk", 1, 5 },
  { "su", 0, 0 },
  { "mo", 1, 1 },
  { "tu", 2, 2 },
  { "we", 3, 3 },
  { "th", 4, 4 },
  { "fr", 5, 5 },
  { "sa", 6, 6 },
  { "never", -1, -2 },
  { "none", -1, -2 },
  { NULL, 0, 0 }
};

/*
 * int isintime(char *z);
 *
 * gibt Anzahl der Sekunden bis zum Verlassen des durch den String z
 * angegebenen Fensters an.
 *
 * isintime=0  : Zeitpunkt nicht im Fenster;
 * isintime=-1 : Fehler (z.B. Syntax Error im Fenster)
 *
 * Georgios Papoutsis, 1995 zum Einbinden in Matthias Urlichs' ISDN
 *
 * Um Mitternacht wird in jedem Fall ein Wechsel getriggert. Das macht nix. ;-)
 */

int
isintime (char *ztime)
{

	char bfirst;
	const char *z;

  	time_t now;
  	struct tm *tm;

  	time (&now);
  	tm = localtime (&now);

  	now = tm->tm_hour * 60 + tm->tm_min;

	/* Look through the time string.  */
	z = ztime;
	while (*z != '\0') {
		int iday;
		char afday[7];
		int istart, iend;

		if (*z == ',' || *z == '|')
			++z;
		if (*z == '\0' || *z == ';')
			break;

		for (iday = 0; iday < 7; iday++)
			afday[iday] = 0;

		/* Get the days.  */
		do {
			bfirst = *z;
			if (isupper (bfirst))
				bfirst = tolower (bfirst);
			for (iday = 0; asTdays[iday].zname != NULL; iday++) {
				size_t clen;

				if (bfirst != asTdays[iday].zname[0])
					continue;

				clen = strlen (asTdays[iday].zname);
				if (strncasecmp (z, asTdays[iday].zname, clen) == 0) {
					int iset;

					for (iset = asTdays[iday].imin;
						iset <= asTdays[iday].imax;
						iset++)
						afday[iset] = 1;
					z += clen;
					break;
				}
			}
			if (asTdays[iday].zname == NULL)
				return -1;
		} while (isalpha (*z));

		/* Get the hours.  */
		if (! isdigit (*z)) {
			istart = 0;
			iend = 24 * 60;
		} else {
			char *zendnum;

			istart = (int) strtol ((char *) z, &zendnum, 10);
			if (*zendnum != '-' || ! isdigit (zendnum[1]))
				return -1;
			z = zendnum + 1;
			iend = (int) strtol ((char *) z, &zendnum, 10);
			z = zendnum;

			istart = (istart / 100) * 60 + istart % 100;
			iend = (iend / 100) * 60 + iend % 100;
		}

		if (afday[tm->tm_wday]) {
			if (istart < iend) {
				if ((istart < now) && (iend > now))
					return (iend - now) * 60;
			} else { /* Wrap around midnight */
				if (iend > now)
					return (24*60-now) * 60;
				if (istart > now)
					return (istart - now) * 60;
			}
		}
	}
	return 0;
}

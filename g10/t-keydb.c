/* t-keydb.c - Tests for keydb.c.
 * Copyright (C) 2015 g10 Code GmbH
 *
 * This file is part of GnuPG.
 *
 * GnuPG is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GnuPG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "test.c"

#include "keydb.h"

static void
do_test (int argc, char *argv[])
{
  int rc;
  KEYDB_HANDLE hd1, hd2;
  KEYDB_SEARCH_DESC desc1, desc2;
  KBNODE kb1, kb2;
  char *uid1;
  char *uid2;
  char *fname;

  (void) argc;
  (void) argv;

  fname = prepend_srcdir ("t-keydb-keyring.kbx");
  rc = keydb_add_resource (fname, 0);
  test_free (fname);
  if (rc)
    ABORT ("Failed to open keyring.");

  hd1 = keydb_new ();
  if (!hd1)
    ABORT ("");
  hd2 = keydb_new ();
  if (!hd2)
    ABORT ("");

  rc = classify_user_id ("2689 5E25 E844 6D44 A26D  8FAF 2F79 98F3 DBFC 6AD9",
			 &desc1, 0);
  if (rc)
    ABORT ("Failed to convert fingerprint for DBFC6AD9");

  rc = keydb_search (hd1, &desc1, 1, NULL);
  if (rc)
    ABORT ("Failed to lookup key associated with DBFC6AD9");


  classify_user_id ("8061 5870 F5BA D690 3336  86D0 F2AD 85AC 1E42 B367",
		    &desc2, 0);
  if (rc)
    ABORT ("Failed to convert fingerprint for 1E42B367");

  rc = keydb_search (hd2, &desc2, 1, NULL);
  if (rc)
    ABORT ("Failed to lookup key associated with 1E42B367");

  rc = keydb_get_keyblock (hd2, &kb2);
  if (rc)
    ABORT ("Failed to get keyblock for 1E42B367");

  rc = keydb_get_keyblock (hd1, &kb1);
  if (rc)
    ABORT ("Failed to get keyblock for DBFC6AD9");

  while (kb1 && kb1->pkt->pkttype != PKT_USER_ID)
    kb1 = kb1->next;
  if (! kb1)
    ABORT ("DBFC6AD9 has no user id packet");
  uid1 = kb1->pkt->pkt.user_id->name;

  while (kb2 && kb2->pkt->pkttype != PKT_USER_ID)
    kb2 = kb2->next;
  if (! kb2)
    ABORT ("1E42B367 has no user id packet");
  uid2 = kb2->pkt->pkt.user_id->name;

  if (verbose)
    {
      printf ("user id for DBFC6AD9: %s\n", uid1);
      printf ("user id for 1E42B367: %s\n", uid2);
    }

  TEST_P ("cache consistency", strcmp (uid1, uid2) != 0);
}

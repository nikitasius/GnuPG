/* sig-check.c -  Check a signature
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003,
 *               2004, 2006 Free Software Foundation, Inc.
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "gpg.h"
#include "util.h"
#include "packet.h"
#include "keydb.h"
#include "main.h"
#include "status.h"
#include "i18n.h"
#include "options.h"
#include "pkglue.h"

static int check_signature_end (PKT_public_key *pk, PKT_signature *sig,
				gcry_md_hd_t digest,
				int *r_expired, int *r_revoked,
				PKT_public_key *ret_pk);

/* Check a signature.  This is shorthand for check_signature2 with
   the unnamed arguments passed as NULL.  */
int
check_signature (PKT_signature *sig, gcry_md_hd_t digest)
{
    return check_signature2 (sig, digest, NULL, NULL, NULL, NULL);
}


/* Check a signature.
 *
 * Looks up the public key that created the signature (SIG->KEYID)
 * from the key db.  Makes sure that the signature is valid (it was
 * not created prior to the key, the public key was created in the
 * past, and the signature does not include any unsupported critical
 * features), finishes computing the hash of the signature data, and
 * checks that the signature verifies the digest.  If the key that
 * generated the signature is a subkey, this function also verifies
 * that there is a valid backsig from the subkey to the primary key.
 * Finally, if status fd is enabled and the signature class is 0x00 or
 * 0x01, then a STATUS_SIG_ID is emitted on the status fd.
 *
 * SIG is the signature to check.
 *
 * DIGEST contains a valid hash context that already includes the
 * signed data.  This function adds the relevant meta-data from the
 * signature packet to compute the final hash.  (See Section 5.2 of
 * RFC 4880: "The concatenation of the data being signed and the
 * signature data from the version number through the hashed subpacket
 * data (inclusive) is hashed.")
 *
 * If R_EXPIREDATE is not NULL, R_EXPIREDATE is set to the key's
 * expiry.
 *
 * If R_EXPIRED is not NULL, *R_EXPIRED is set to 1 if PK has expired
 * (0 otherwise).  Note: PK being expired does not cause this function
 * to fail.
 *
 * If R_REVOKED is not NULL, *R_REVOKED is set to 1 if PK has been
 * revoked (0 otherwise).  Note: PK being revoked does not cause this
 * function to fail.
 *
 * If PK is not NULL, the public key is saved in *PK on success.
 *
 * Returns 0 on success.  An error code otherwise.  */
int
check_signature2 (PKT_signature *sig, gcry_md_hd_t digest, u32 *r_expiredate,
		  int *r_expired, int *r_revoked, PKT_public_key *pk )
{
    int rc=0;
    int pk_internal;

    if (pk)
      pk_internal = 0;
    else
      {
	pk_internal = 1;
	pk = xmalloc_clear( sizeof *pk );
      }

    if ( (rc=openpgp_md_test_algo(sig->digest_algo)) )
      ; /* We don't have this digest. */
    else if ((rc=openpgp_pk_test_algo(sig->pubkey_algo)))
      ; /* We don't have this pubkey algo. */
    else if (!gcry_md_is_enabled (digest,sig->digest_algo))
      {
	/* Sanity check that the md has a context for the hash that the
	   sig is expecting.  This can happen if a onepass sig header does
	   not match the actual sig, and also if the clearsign "Hash:"
	   header is missing or does not match the actual sig. */

        log_info(_("WARNING: signature digest conflict in message\n"));
	rc = GPG_ERR_GENERAL;
      }
    else if( get_pubkey( pk, sig->keyid ) )
	rc = GPG_ERR_NO_PUBKEY;
    else if(!pk->flags.valid && !pk->flags.primary)
      {
        /* You cannot have a good sig from an invalid subkey.  */
        rc = GPG_ERR_BAD_PUBKEY;
      }
    else
      {
        if(r_expiredate)
	  *r_expiredate = pk->expiredate;

	rc = check_signature_end (pk, sig, digest, r_expired, r_revoked, NULL);

	/* Check the backsig.  This is a 0x19 signature from the
	   subkey on the primary key.  The idea here is that it should
	   not be possible for someone to "steal" subkeys and claim
	   them as their own.  The attacker couldn't actually use the
	   subkey, but they could try and claim ownership of any
	   signatures issued by it. */
	if(rc==0 && !pk->flags.primary && pk->flags.backsig < 2)
	  {
	    if (!pk->flags.backsig)
	      {
		log_info(_("WARNING: signing subkey %s is not"
			   " cross-certified\n"),keystr_from_pk(pk));
		log_info(_("please see %s for more information\n"),
			 "https://gnupg.org/faq/subkey-cross-certify.html");
		/* --require-cross-certification makes this warning an
                     error.  TODO: change the default to require this
                     after more keys have backsigs. */
		if(opt.flags.require_cross_cert)
		  rc = GPG_ERR_GENERAL;
	      }
	    else if(pk->flags.backsig == 1)
	      {
		log_info(_("WARNING: signing subkey %s has an invalid"
			   " cross-certification\n"),keystr_from_pk(pk));
		rc = GPG_ERR_GENERAL;
	      }
	  }
      }

    if (pk_internal || rc)
      {
	release_public_key_parts (pk);
	if (pk_internal)
	  xfree (pk);
	else
	  /* Be very sure that the caller doesn't try to use *PK.  */
	  memset (pk, 0, sizeof (*pk));
      }

    if( !rc && sig->sig_class < 2 && is_status_enabled() ) {
	/* This signature id works best with DLP algorithms because
	 * they use a random parameter for every signature.  Instead of
	 * this sig-id we could have also used the hash of the document
	 * and the timestamp, but the drawback of this is, that it is
	 * not possible to sign more than one identical document within
	 * one second.	Some remote batch processing applications might
	 * like this feature here.
         *
         * Note that before 2.0.10, we used RIPE-MD160 for the hash
         * and accidentally didn't include the timestamp and algorithm
         * information in the hash.  Given that this feature is not
         * commonly used and that a replay attacks detection should
         * not solely be based on this feature (because it does not
         * work with RSA), we take the freedom and switch to SHA-1
         * with 2.0.10 to take advantage of hardware supported SHA-1
         * implementations.  We also include the missing information
         * in the hash.  Note also the SIG_ID as computed by gpg 1.x
         * and gpg 2.x didn't matched either because 2.x used to print
         * MPIs not in PGP format.  */
	u32 a = sig->timestamp;
	int nsig = pubkey_get_nsig( sig->pubkey_algo );
	unsigned char *p, *buffer;
        size_t n, nbytes;
        int i;
        char hashbuf[20];

        nbytes = 6;
	for (i=0; i < nsig; i++ )
          {
	    if (gcry_mpi_print (GCRYMPI_FMT_USG, NULL, 0, &n, sig->data[i]))
              BUG();
            nbytes += n;
          }

        /* Make buffer large enough to be later used as output buffer.  */
        if (nbytes < 100)
          nbytes = 100;
        nbytes += 10;  /* Safety margin.  */

        /* Fill and hash buffer.  */
        buffer = p = xmalloc (nbytes);
	*p++ = sig->pubkey_algo;
	*p++ = sig->digest_algo;
	*p++ = (a >> 24) & 0xff;
	*p++ = (a >> 16) & 0xff;
	*p++ = (a >>  8) & 0xff;
	*p++ =  a & 0xff;
        nbytes -= 6;
	for (i=0; i < nsig; i++ )
          {
	    if (gcry_mpi_print (GCRYMPI_FMT_PGP, p, nbytes, &n, sig->data[i]))
              BUG();
            p += n;
            nbytes -= n;
          }
        gcry_md_hash_buffer (GCRY_MD_SHA1, hashbuf, buffer, p-buffer);

	p = make_radix64_string (hashbuf, 20);
	sprintf (buffer, "%s %s %lu",
		 p, strtimestamp (sig->timestamp), (ulong)sig->timestamp);
	xfree (p);
	write_status_text (STATUS_SIG_ID, buffer);
	xfree (buffer);
    }

    return rc;
}


/* The signature SIG was generated with the public key PK.  Check
 * whether the signature is valid in the following sense:
 *
 *   - Make sure the public key was created before the signature was
 *     generated.
 *
 *   - Make sure the public key was created in the past
 *
 *   - Check whether PK has expired (set *R_EXPIRED to 1 if so and 0
 *     otherwise)
 *
 *   - Check whether PK has been revoked (set *R_REVOKED to 1 if so
 *     and 0 otherwise).
 *
 * If either of the first two tests fail, returns an error code.
 * Otherwise returns 0.  (Thus, this function doesn't fail if the
 * public key is expired or revoked.)  */
static int
check_signature_metadata_validity (PKT_public_key *pk, PKT_signature *sig,
				   int *r_expired, int *r_revoked)
{
    u32 cur_time;

    if(r_expired)
      *r_expired = 0;
    if(r_revoked)
      *r_revoked = 0;

    if( pk->timestamp > sig->timestamp )
      {
	ulong d = pk->timestamp - sig->timestamp;
        if ( d < 86400 )
          {
            log_info
              (ngettext
               ("public key %s is %lu second newer than the signature\n",
                "public key %s is %lu seconds newer than the signature\n",
                d), keystr_from_pk (pk), d);
          }
        else
          {
            d /= 86400;
            log_info
              (ngettext
               ("public key %s is %lu day newer than the signature\n",
                "public key %s is %lu days newer than the signature\n",
                d), keystr_from_pk (pk), d);
          }
	if (!opt.ignore_time_conflict)
	  return GPG_ERR_TIME_CONFLICT; /* pubkey newer than signature.  */
      }

    cur_time = make_timestamp();
    if( pk->timestamp > cur_time )
      {
	ulong d = pk->timestamp - cur_time;
        if (d < 86400)
          {
            log_info (ngettext("key %s was created %lu second"
                               " in the future (time warp or clock problem)\n",
                               "key %s was created %lu seconds"
                               " in the future (time warp or clock problem)\n",
                               d), keystr_from_pk (pk), d);
          }
        else
          {
            d /= 86400;
            log_info (ngettext("key %s was created %lu day"
                               " in the future (time warp or clock problem)\n",
                               "key %s was created %lu days"
                               " in the future (time warp or clock problem)\n",
                               d), keystr_from_pk (pk), d);
          }
	if (!opt.ignore_time_conflict)
	  return GPG_ERR_TIME_CONFLICT;
      }

    /* Check whether the key has expired.  We check the has_expired
       flag which is set after a full evaluation of the key (getkey.c)
       as well as a simple compare to the current time in case the
       merge has for whatever reasons not been done.  */
    if( pk->has_expired || (pk->expiredate && pk->expiredate < cur_time)) {
        char buf[11];
        if (opt.verbose)
	  log_info(_("Note: signature key %s expired %s\n"),
		   keystr_from_pk(pk), asctimestamp( pk->expiredate ) );
	sprintf(buf,"%lu",(ulong)pk->expiredate);
	write_status_text(STATUS_KEYEXPIRED,buf);
	if(r_expired)
	  *r_expired = 1;
    }

    if (pk->flags.revoked)
      {
        if (opt.verbose)
	  log_info (_("Note: signature key %s has been revoked\n"),
                    keystr_from_pk(pk));
        if (r_revoked)
          *r_revoked=1;
      }

    return 0;
}


/* Finish generating a signature and check it.  Concretely: make sure
 * that the signature is valid (it was not created prior to the key,
 * the public key was created in the past, and the signature does not
 * include any unsupported critical features), finish computing the
 * digest by adding the relevant data from the signature packet, and
 * check that the signature verifies the digest.
 *
 * DIGEST contains a hash context, which has already hashed the signed
 * data.  This function adds the relevant meta-data from the signature
 * packet to compute the final hash.  (See Section 5.2 of RFC 4880:
 * "The concatenation of the data being signed and the signature data
 * from the version number through the hashed subpacket data
 * (inclusive) is hashed.")
 *
 * SIG is the signature to check.
 *
 * PK is the public key used to generate the signature.
 *
 * If R_EXPIRED is not NULL, *R_EXPIRED is set to 1 if PK has expired
 * (0 otherwise).  Note: PK being expired does not cause this function
 * to fail.
 *
 * If R_REVOKED is not NULL, *R_REVOKED is set to 1 if PK has been
 * revoked (0 otherwise).  Note: PK being revoked does not cause this
 * function to fail.
 *
 * If RET_PK is not NULL, PK is copied into RET_PK on success.
 *
 * Returns 0 on success.  An error code other.  */
static int
check_signature_end (PKT_public_key *pk, PKT_signature *sig,
		     gcry_md_hd_t digest,
		     int *r_expired, int *r_revoked, PKT_public_key *ret_pk)
{
    gcry_mpi_t result = NULL;
    int rc = 0;
    const struct weakhash *weak;

    if ((rc = check_signature_metadata_validity (pk, sig,
						 r_expired, r_revoked)))
        return rc;

    if (!opt.flags.allow_weak_digest_algos)
      for (weak = opt.weak_digests; weak; weak = weak->next)
        if (sig->digest_algo == weak->algo)
          {
            print_digest_rejected_note(sig->digest_algo);
            return GPG_ERR_DIGEST_ALGO;
          }

    /* Make sure the digest algo is enabled (in case of a detached
       signature).  */
    gcry_md_enable (digest, sig->digest_algo);

    /* Complete the digest. */
    if( sig->version >= 4 )
	gcry_md_putc( digest, sig->version );
    gcry_md_putc( digest, sig->sig_class );
    if( sig->version < 4 ) {
	u32 a = sig->timestamp;
	gcry_md_putc( digest, (a >> 24) & 0xff );
	gcry_md_putc( digest, (a >> 16) & 0xff );
	gcry_md_putc( digest, (a >>	8) & 0xff );
	gcry_md_putc( digest,  a	   & 0xff );
    }
    else {
	byte buf[6];
	size_t n;
	gcry_md_putc( digest, sig->pubkey_algo );
	gcry_md_putc( digest, sig->digest_algo );
	if( sig->hashed ) {
	    n = sig->hashed->len;
            gcry_md_putc (digest, (n >> 8) );
            gcry_md_putc (digest,  n       );
	    gcry_md_write (digest, sig->hashed->data, n);
	    n += 6;
	}
	else {
	  /* Two octets for the (empty) length of the hashed
             section. */
          gcry_md_putc (digest, 0);
	  gcry_md_putc (digest, 0);
	  n = 6;
	}
	/* add some magic per Section 5.2.4 of RFC 4880.  */
	buf[0] = sig->version;
	buf[1] = 0xff;
	buf[2] = n >> 24;
	buf[3] = n >> 16;
	buf[4] = n >>  8;
	buf[5] = n;
	gcry_md_write( digest, buf, 6 );
    }
    gcry_md_final( digest );

    /* Convert the digest to an MPI.  */
    result = encode_md_value (pk, digest, sig->digest_algo );
    if (!result)
        return GPG_ERR_GENERAL;

    /* Verify the signature.  */
    rc = pk_verify( pk->pubkey_algo, result, sig->data, pk->pkey );
    gcry_mpi_release (result);

    if( !rc && sig->flags.unknown_critical )
      {
	log_info(_("assuming bad signature from key %s"
		   " due to an unknown critical bit\n"),keystr_from_pk(pk));
	rc = GPG_ERR_BAD_SIGNATURE;
      }

    if(!rc && ret_pk)
      copy_public_key(ret_pk,pk);

    return rc;
}


/* Add a uid node to a hash context.  See section 5.2.4, paragraph 4
   of RFC 4880.  */
static void
hash_uid_node( KBNODE unode, gcry_md_hd_t md, PKT_signature *sig )
{
    PKT_user_id *uid = unode->pkt->pkt.user_id;

    assert( unode->pkt->pkttype == PKT_USER_ID );
    if( uid->attrib_data ) {
	if( sig->version >=4 ) {
	    byte buf[5];
	    buf[0] = 0xd1;		     /* packet of type 17 */
	    buf[1] = uid->attrib_len >> 24;  /* always use 4 length bytes */
	    buf[2] = uid->attrib_len >> 16;
	    buf[3] = uid->attrib_len >>  8;
	    buf[4] = uid->attrib_len;
	    gcry_md_write( md, buf, 5 );
	}
	gcry_md_write( md, uid->attrib_data, uid->attrib_len );
    }
    else {
	if( sig->version >=4 ) {
	    byte buf[5];
	    buf[0] = 0xb4;	      /* indicates a userid packet */
	    buf[1] = uid->len >> 24;  /* always use 4 length bytes */
	    buf[2] = uid->len >> 16;
	    buf[3] = uid->len >>  8;
	    buf[4] = uid->len;
	    gcry_md_write( md, buf, 5 );
	}
	gcry_md_write( md, uid->name, uid->len );
    }
}

static void
cache_sig_result ( PKT_signature *sig, int result )
{
    if ( !result ) {
        sig->flags.checked = 1;
        sig->flags.valid = 1;
    }
    else if ( gpg_err_code (result) == GPG_ERR_BAD_SIGNATURE ) {
        sig->flags.checked = 1;
        sig->flags.valid = 0;
    }
    else {
        sig->flags.checked = 0;
        sig->flags.valid = 0;
    }
}


/* SIG is a key revocation signature.  Check if this signature was
 * generated by any of the public key PK's designated revokers.
 *
 *   PK is the public key that SIG allegedly revokes.
 *
 *   SIG is the revocation signature to check.
 *
 * This function avoids infinite recursion, which can happen if two
 * keys are designed revokers for each other and they revoke each
 * other.  This is done by observing that if a key A is revoked by key
 * B we still consider the revocation to be valid even if B is
 * revoked.  Thus, we don't need to determine whether B is revoked to
 * determine whether A has been revoked by B, we just need to check
 * the signature.
 *
 * Returns 0 if sig is valid (i.e. pk is revoked), non-0 if not
 * revoked.  We are careful to make sure that GPG_ERR_NO_PUBKEY is
 * only returned when a revocation signature is from a valid
 * revocation key designated in a revkey subpacket, but the revocation
 * key itself isn't present.
 *
 * XXX: This code will need to be modified if gpg ever becomes
 * multi-threaded.  Note that this guarantees that a designated
 * revocation sig will never be considered valid unless it is actually
 * valid, as well as being issued by a revocation key in a valid
 * direct signature.  Note also that this is written so that a revoked
 * revoker can still issue revocations: i.e. If A revokes B, but A is
 * revoked, B is still revoked.  I'm not completely convinced this is
 * the proper behavior, but it matches how PGP does it. -dms */
int
check_revocation_keys (PKT_public_key *pk, PKT_signature *sig)
{
  static int busy=0;
  int i;
  int rc = GPG_ERR_GENERAL;

  assert(IS_KEY_REV(sig));
  assert((sig->keyid[0]!=pk->keyid[0]) || (sig->keyid[0]!=pk->keyid[1]));

  /* Avoid infinite recursion.  Consider the following:
   *
   *   - We want to check if A is revoked.
   *
   *   - C is a designated revoker for B and has revoked B.
   *
   *   - B is a designated revoker for A and has revoked A.
   *
   * When checking if A is revoked (in merge_selfsigs_main), we
   * observe that A has a designed revoker.  As such, we call this
   * function.  This function sees that there is a valid revocation
   * signature, which is signed by B.  It then calls check_signature()
   * to verify that the signature is good.  To check the sig, we need
   * to lookup B.  Looking up B means calling merge_selfsigs_main,
   * which checks whether B is revoked, which calls this function to
   * see if B was revoked by some key.
   *
   * In this case, the added level of indirection doesn't hurt.  It
   * just means a bit more work.  However, if C == A, then we'd end up
   * in a loop.  But, it doesn't make sense to look up C anyways: even
   * if B is revoked, we conservatively consider a valid revocation
   * signed by B to revoke A.  Since this is the only place where this
   * type of recursion can occur, we simply cause this function to
   * fail if it is entered recursively.  */
  if (busy)
    {
      /* Return an error (i.e. not revoked), but mark the pk as
         uncacheable as we don't really know its revocation status
         until it is checked directly.  */
      pk->flags.dont_cache = 1;
      return rc;
    }

  busy=1;

  /*  es_printf("looking at %08lX with a sig from %08lX\n",(ulong)pk->keyid[1],
      (ulong)sig->keyid[1]); */

  /* is the issuer of the sig one of our revokers? */
  if( !pk->revkey && pk->numrevkeys )
     BUG();
  else
      for(i=0;i<pk->numrevkeys;i++)
	{
	  /* The revoker's keyid.  */
          u32 keyid[2];

          keyid_from_fingerprint(pk->revkey[i].fpr,MAX_FINGERPRINT_LEN,keyid);

          if(keyid[0]==sig->keyid[0] && keyid[1]==sig->keyid[1])
	    /* The signature was generated by a designated revoker.
	       Verify the signature.  */
	    {
              gcry_md_hd_t md;

              if (gcry_md_open (&md, sig->digest_algo, 0))
                BUG ();
              hash_public_key(md,pk);
	      /* Note: check_signature only checks that the signature
		 is good.  It does not fail if the key is revoked.  */
              rc=check_signature(sig,md);
	      cache_sig_result(sig,rc);
              gcry_md_close (md);
	      break;
	    }
	}

  busy=0;

  return rc;
}

/* Check that the backsig BACKSIG from the subkey SUB_PK to its
   primary key MAIN_PK is valid.

   Backsigs (0x19) have the same format as binding sigs (0x18), but
   this function is simpler than check_key_signature in a few ways.
   For example, there is no support for expiring backsigs since it is
   questionable what such a thing actually means.  Note also that the
   sig cache check here, unlike other sig caches in GnuPG, is not
   persistent. */
int
check_backsig (PKT_public_key *main_pk,PKT_public_key *sub_pk,
	       PKT_signature *backsig)
{
  gcry_md_hd_t md;
  int rc;

  /* Always check whether the algorithm is available.  Although
     gcry_md_open would throw an error, some libgcrypt versions will
     print a debug message in that case too. */
  if ((rc=openpgp_md_test_algo (backsig->digest_algo)))
    return rc;

  if(!opt.no_sig_cache && backsig->flags.checked)
    return backsig->flags.valid? 0 : gpg_error (GPG_ERR_BAD_SIGNATURE);

  rc = gcry_md_open (&md, backsig->digest_algo,0);
  if (!rc)
    {
      hash_public_key(md,main_pk);
      hash_public_key(md,sub_pk);
      rc = check_signature_end (sub_pk, backsig, md, NULL, NULL, NULL);
      cache_sig_result(backsig,rc);
      gcry_md_close(md);
    }

  return rc;
}


/* Check that a signature over a key is valid.  This is a
 * specialization of check_key_signature2 with the unnamed parameters
 * passed as NULL.  See the documentation for that function for more
 * details.  */
int
check_key_signature (KBNODE root, KBNODE node, int *is_selfsig)
{
  return check_key_signature2 (root, node, NULL, NULL, is_selfsig, NULL, NULL);
}


/* Check that a signature over a key (e.g., a key revocation, key
 * binding, user id certification, etc.) is valid.  If the function
 * detects a self-signature, it uses the public key from the specified
 * key block and does not bother looking up the key specified in the
 * signature packet.
 *
 * ROOT is a keyblock.
 *
 * NODE references a signature packet that appears in the keyblock
 * that should be verified.
 *
 * If CHECK_PK is set, the specified key is sometimes preferred for
 * verifying signatures.  See the implementation for details.
 *
 * If RET_PK is not NULL, the public key that successfully verified
 * the signature is copied into *RET_PK.
 *
 * If IS_SELFSIG is not NULL, *IS_SELFSIG is set to 1 if NODE is a
 * self-signature.
 *
 * If R_EXPIREDATE is not NULL, *R_EXPIREDATE is set to the expiry
 * date.
 *
 * If R_EXPIRED is not NULL, *R_EXPIRED is set to 1 if PK has been
 * expired (0 otherwise).  Note: PK being revoked does not cause this
 * function to fail.
 *
 *
 * If OPT.NO_SIG_CACHE is not set, this function will first check if
 * the result of a previous verification is already cached in the
 * signature packet's data structure.
 *
 * TODO: add r_revoked here as well.  It has the same problems as
 * r_expiredate and r_expired and the cache.  */
int
check_key_signature2 (kbnode_t root, kbnode_t node, PKT_public_key *check_pk,
                      PKT_public_key *ret_pk, int *is_selfsig,
                      u32 *r_expiredate, int *r_expired )
{
  gcry_md_hd_t md;
  PKT_public_key *pk;
  PKT_signature *sig;
  int algo;
  int rc;

  if (is_selfsig)
    *is_selfsig = 0;
  if (r_expiredate)
    *r_expiredate = 0;
  if (r_expired)
    *r_expired = 0;
  assert (node->pkt->pkttype == PKT_SIGNATURE);
  assert (root->pkt->pkttype == PKT_PUBLIC_KEY);

  pk = root->pkt->pkt.public_key;
  sig = node->pkt->pkt.signature;
  algo = sig->digest_algo;

  /* Check whether we have cached the result of a previous signature
     check.  Note that we may no longer have the pubkey or hash
     needed to verify a sig, but can still use the cached value.  A
     cache refresh detects and clears these cases. */
  if ( !opt.no_sig_cache )
    {
      if (sig->flags.checked) /* Cached status available.  */
        {
          if (is_selfsig)
            {
              u32 keyid[2];

              keyid_from_pk (pk, keyid);
              if (keyid[0] == sig->keyid[0] && keyid[1] == sig->keyid[1])
                *is_selfsig = 1;
	    }
          /* BUG: This is wrong for non-self-sigs... needs to be the
             actual pk.  */
          rc = check_signature_metadata_validity (pk, sig, r_expired, NULL);
          if (rc)
            return rc;
          return sig->flags.valid? 0 : gpg_error (GPG_ERR_BAD_SIGNATURE);
        }
    }

  rc = openpgp_pk_test_algo(sig->pubkey_algo);
  if (rc)
    return rc;
  rc = openpgp_md_test_algo(algo);
  if (rc)
    return rc;

  if (sig->sig_class == 0x20) /* key revocation */
    {
      u32 keyid[2];
      keyid_from_pk( pk, keyid );

      /* Is it a designated revoker? */
      if (keyid[0] != sig->keyid[0] || keyid[1] != sig->keyid[1])
        rc = check_revocation_keys (pk, sig);
      else
        {
          if (gcry_md_open (&md, algo, 0))
            BUG ();
          hash_public_key (md, pk);
          rc = check_signature_end (pk, sig, md, r_expired, NULL, ret_pk);
          cache_sig_result (sig, rc);
          gcry_md_close (md);
        }
    }
  else if (sig->sig_class == 0x28) /* subkey revocation */
    {
      kbnode_t snode = find_prev_kbnode (root, node, PKT_PUBLIC_SUBKEY);

      if (snode)
        {
          if (gcry_md_open (&md, algo, 0))
            BUG ();
          hash_public_key (md, pk);
          hash_public_key (md, snode->pkt->pkt.public_key);
          rc = check_signature_end (pk, sig, md, r_expired, NULL, ret_pk);
          cache_sig_result (sig, rc);
          gcry_md_close (md);
	}
      else
        {
          if (opt.verbose)
            log_info (_("key %s: no subkey for subkey"
                        " revocation signature\n"), keystr_from_pk(pk));
          rc = GPG_ERR_SIG_CLASS;
        }
    }
    else if (sig->sig_class == 0x18) /* key binding */
      {
	kbnode_t snode = find_prev_kbnode (root, node, PKT_PUBLIC_SUBKEY);

	if (snode)
          {
	    if (is_selfsig)
              {
                /* Does this make sense?  It should always be a
                   selfsig.  Yes: We can't be sure about this and we
                   need to be able to indicate that it is a selfsig.
                   FIXME: The question is whether we should reject
                   such a signature if it is not a selfsig.  */
		u32 keyid[2];

		keyid_from_pk (pk, keyid);
		if (keyid[0] == sig->keyid[0] && keyid[1] == sig->keyid[1])
                  *is_selfsig = 1;
              }
	    if (gcry_md_open (&md, algo, 0))
              BUG ();
	    hash_public_key (md, pk);
	    hash_public_key (md, snode->pkt->pkt.public_key);
	    rc = check_signature_end (pk, sig, md, r_expired, NULL, ret_pk);
            cache_sig_result ( sig, rc );
	    gcry_md_close (md);
          }
	else
	  {
            if (opt.verbose)
	      log_info(_("key %s: no subkey for subkey"
			 " binding signature\n"), keystr_from_pk(pk));
	    rc = GPG_ERR_SIG_CLASS;
	  }
      }
    else if (sig->sig_class == 0x1f) /* direct key signature */
      {
        if (gcry_md_open (&md, algo, 0 ))
          BUG ();
	hash_public_key( md, pk );
	rc = check_signature_end (pk, sig, md, r_expired, NULL, ret_pk);
        cache_sig_result (sig, rc);
	gcry_md_close (md);
      }
    else /* all other classes */
      {
	kbnode_t unode = find_prev_kbnode (root, node, PKT_USER_ID);

	if (unode)
          {
	    u32 keyid[2];

	    keyid_from_pk (pk, keyid);
	    if (gcry_md_open (&md, algo, 0))
              BUG ();
	    hash_public_key (md, pk);
	    hash_uid_node (unode, md, sig);
	    if (keyid[0] == sig->keyid[0] && keyid[1] == sig->keyid[1])
	      { /* The primary key is the signing key.  */

		if (is_selfsig)
		  *is_selfsig = 1;
		rc = check_signature_end (pk, sig, md, r_expired, NULL, ret_pk);
	      }
	    else if (check_pk)
              { /* The caller specified a key.  Try that.  */

                rc = check_signature_end (check_pk, sig, md,
                                          r_expired, NULL, ret_pk);
              }
	    else
              { /* Look up the key.  */
                rc = check_signature2 (sig, md, r_expiredate, r_expired,
                                       NULL, ret_pk);
              }

            cache_sig_result  (sig, rc);
	    gcry_md_close (md);
          }
	else
	  {
            if (!opt.quiet)
	      log_info ("key %s: no user ID for key signature packet"
			" of class %02x\n",keystr_from_pk(pk),sig->sig_class);
	    rc = GPG_ERR_SIG_CLASS;
	  }
      }

  return rc;
}

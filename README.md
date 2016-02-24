# GnuPG with large RSA keys

#There is a BUG, im testing with modified libgcrypt.

##WARNING
###Before installation you **must** export **ALL** your pub/sec keys from current keyring and delete keyring! *CHECKING*
Explanation you can find in Errors section.
## Description
GnuPG with large RSA keys (up to 32768 bytes).

This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website.

## Preparation
You need enought entropy. Check your current entropy level you can via `cat /proc/sys/kernel/random/entropy_avail`.

Your maximum level of entropy here: `cat /proc/sys/kernel/random/poolsize`. For last linux distro we have `4096` as poolsize value.
You in case when you limited with 4096 you should have 3000+ entropy. During all tests my laptop had 3100-3200 entropy.
To increase your entropy level you can install `rng-tools` and `haveged`.

`rng-tools` - there is many guides how to tune it. After installing it will boost well your entropy level. Same time if your PC support TPM and you have hardware random generators, you can tune `rng-tools` to use it. In another case it will use `rdrand` CPU flag (if your CPU have it) to boost entropy level.

`haveged` - i don't have TPM module in my laptop, so i use `haveged` to boost my entropy level with `rng-tools` same time. `haveged` run with default param `1024`. Without `haveged` i have 1600-2000 entropy, so i've set up `3072` for `haveged` service to have at least 3100 of entropy. Each time when you configure `haveged` check CPU consumption, because it's a software generator and if you don't have entropy as is, it will consume a lot of CPU time. In my case with i5-5200U and `3072` for `haveged` all cores have 3%-7% in idle time on `4.3.0-0.bpo.1-amd64 #1 SMP Debian 4.3.3-7~bpo8+1 (2016-01-19) x86_64 GNU/Linux`.

## Configuration
### Debian
Update your aptitude: `aptitude update`

Install packets for compilation: `aptitude install gcc make checkinstall`

To build with BZIP2 support you need: `aptitude install libbz2-dev`

Download and execute from folder: `./autogen.sh`, it will show you how to run `./configure`

To see help run `./configure --help`

##Compilation
If all is how you want, you can run `make`

##Tests
To pass the tests run `make check`

I've tested on my laptop: RSA1024-OK, RSA2048-OK,RSA3072-OK, RSA4096-OK, RSA8192-OK, RSA16384-OK (19min), RSA32768-OK (2h).

##Installation
If all tests are passed well, execute`checkinstall` and fill the fields like below:

> 1 -  Summary: [ gnupg2-2.0.29 ]

> 2 -  Name:    [ gnupg2 ]

> 3 -  Version: [ 2.0.29 ]

> 11 - Provides: [ gnupg2 ]

After this you will be able to delete it via gpkg as `dpkg -r gnupg2`.

##Errors
###gpg: problem with the agent: No pinentry
This problem common for all versions of GnuPG which was installed manually and here is 2 solutions:

 1) delete `gnupg-agent` and `gpa` from previous version: `aptitude remove gnupg-agent gpa`. After you need configure/compile/install this gnupg again which will install correcly **new** version of gnupg-agent. Process `gnupg-agent` **must be stopped**! 
 
 2) install pinentry, for example `pinentry-curses`: `aptitude install pinentry-curses` and configure your gnupg to use this as adding `--with-pinentry-pgm=/usr/bin/pinentry-curses` to `./configure`.
 
**Both** solution can work, but i recommend to start with solution#1, and if it changed nothing, apply solution#2. In my case i've used both solution, because i prefer curses as pinentry.
###Keyring problems *UNCONFIRMED* - TESTING. After libgcrypt fix seems nice. So i need to generate again keys on vanilla and on modified versions to know did they affect keyring.
As i've written above: before installation you must export ALL your current pub/sec keys from your current keyring and DELETE your current keyring! We are changed packet size, so it will be impossible to work with OLD keyring.

> gpg: checking the trustdb

> gpg: keyring_get_keyblock: read error: Invalid packet

> gpg: keyring_get_keyblock failed: Invalid keyring

> gpg: failed to rebuild keyring cache: Invalid keyring

> gpg: keydb_search failed: Invalid packet

> gpg: public key of ultimately trusted key C6890411 not found

> gpg: keydb_search failed: Invalid packet

> gpg: public key of ultimately trusted key 8BF0E8A4 not found

> gpg: keyring_get_keyblock: read error: Invalid packet

> gpg: keydb_get_keyblock failed: Invalid keyring

> gpg: keydb_search failed: Invalid keyring

> gpg: public key of ultimately trusted key BE98D714 not found

> gpg: 3 marginal(s) needed, 1 complete(s) needed, PGP trust model

> gpg: keyring_get_keyblock: read error: Invalid packet

> gpg: keydb_get_keyblock failed: Invalid keyring

> gpg: validate_key_list failed

> /home/USERNAME/.gnupg/secring.gpg


##This is devel branch, so it's under testing now.
# GnuPG (2.0.29) with large RSA keys (up to 32768)

## WARNING
## USE MODIFIED GNUPG AT YOUR OWN RISK. SOFTWARE MAY CAUSE DATA LOSS, SYSTEM CRASHES, AND RED EYES.
### Large keys, created in modified GnuPG with modified libgcrypt CANNOT be read by vanilla versions! It mean, that if you have another PGP stuff in your PC which work with keys, you should to export keys OR create another keyring for vanilla versions.

## Description
GnuPG with large RSA keys support (up to 32768 bytes).

This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website. To start i copied original GnuPG into the branch `gnupg-2.0.29`, where it stays **non**-modified. After it goes into `devel` branch where im working with. And after it goes into `2.0.29-RSA32k` branch, where can be downloaded.

## Preparation
###Debian
You need enought entropy. You can check your current entropy level via `cat /proc/sys/kernel/random/entropy_avail`.

Your maximum entropy level here: `cat /proc/sys/kernel/random/poolsize`. For last linux distro we have `4096` as poolsize value. For better keys generation you should have 3000+.

To increase your entropy level you can install `rng-tools` and `haveged`.

`rng-tools` - there is many guides how to tune it. Being once installed it will boost well your entropy level. Same time if your PC support TPM and you have *hardware random generators*, you can tune `rng-tools` to use them. In another case it will use `rdrand` CPU flag (if your CPU have it) to boost entropy level.

`haveged` - i don't have TPM module in my laptop, so i use `haveged` to boost my entropy level with `rng-tools` same time. `haveged` run with default param `1024`. Without `haveged` (but with `rng-tools`) i had 1600-2000 entropy, so i've increased from `1024` to `3072` for `haveged` service to have at least 3100 of entropy. Each time when you configure `haveged` **check CPU consumption**, because it's a software generator and if you don't have entropy as is, it will consume a lot of CPU time. In my case with i5-5200U and `3072` for `haveged` all cores have 3%-7% in idle time on `4.3.0-0.bpo.1-amd64 #1 SMP Debian 4.3.3-7~bpo8+1 (2016-01-19) x86_64 GNU/Linux`.

## Configuration
## Patch vanilla gnupg-2.0.29
If by some private reasons you don't want to download this version from this Github repo, you can download patch [gnupg-2.0.29-RSA32k.patch](https://raw.githubusercontent.com/nikitasius/GnuPG/2.0.29-RSA32k/gnupg-2.0.29-RSA32k.patch) and download [vanilla gnupg-2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2).

After you can check patch content and if all is ok, copy it inside folder with vanilla gnupg-2.0.29 and run `patch -p1 < gnupg-2.0.29-RSA32k.patch`. After patching your gnupg-2.0.29 will be able to work with RSA-32768 keys.

### Debian
Update your aptitude: `aptitude update`

Install packets for compilation: `aptitude install gcc make checkinstall`

Install [**modified** libgcrypt](https://github.com/nikitasius/libgcrypt).

To build with BZIP2 support you need: `aptitude install libbz2-dev`

Download and execute from folder: `./configure`

To see help and detail configuration run `./configure --help`


##Compilation
If all is how you want, you can run `make`

##Tests
To pass the tests run `make check`

I've tested on my laptop: RSA1024-OK, RSA2048-OK,RSA3072-OK, RSA4096-OK, RSA8192-OK, RSA16384-OK, RSA32768-OK.

##Installation
If all tests are passed well, execute`checkinstall` and fill the fields like below:

> 1 -  Summary: [ gnupg2-2.0.29 ]

> 2 -  Name:    [ gnupg2 ]

> 3 -  Version: [ 2.0.29 ]

> 11 - Provides: [ gnupg2 ]

After this you will be able to delete it via gpkg as `dpkg -r gnupg2` **or** run `make install` if you do not use `checkinstall`.

##Perfomance
On i5-5200U laptop under debian 8.3 (`4.3.0-0.bpo.1-amd64 #1 SMP Debian 4.3.3-7~bpo8+1 (2016-01-19) x86_64 GNU/Linux`)

>RSA 16384 - 19 minutes

>RSA 32768 - 106 minutes

**Encryption with RSA 32k** file.gz - 12Mb file from debian [ls-lR.gz](http://ftp.debian.org/debian/ls-lR.gz)

> time gpg2 --out file.gz.enc --recipient "test32768pair" --encrypt file.gz

>

>real	0m0.079s

>user	0m0.072s

>sys	0m0.004s

**decryption with RSA 32k**

> time gpg2 --out file.gz.gz --decrypt file.gz.enc

>

>real	0m7.610s

>user	0m5.624s

>sys	0m0.024s

**sha1sum file.* **

>7ab98fd4a154fad5f5bbe0d698178783cd2ac994  file.gz

>9773bb1b9d7f75f408f562d476e8936aafa0f3b9  file.gz.enc

>7ab98fd4a154fad5f5bbe0d698178783cd2ac994  file.gz.gz

##Errors
###gpg: problem with the agent: No pinentry
This problem common for all versions of GnuPG (modified and vanilla) which was installed manually and here is **two solutions**:

 1) delete `gnupg-agent` and `gpa` from previous version: `aptitude remove gnupg-agent gpa`. After you need configure/compile/install modified GnuPG again which will install correcly **new** version of gnupg-agent. Process `gnupg-agent` **must be stopped**! 
 
 2) install 3rd-party pinentry, for example `pinentry-curses`: `aptitude install pinentry-curses` and configure your gnupg to use this as adding `--with-pinentry-pgm=/usr/bin/pinentry-curses` to `./configure`. After you need to create in `.gnupg` folder (which in your HOME directory) file `gpg-agent.conf` with `pinentry-program /usr/bin/pinentry-curses` and restart `gnupg-agent` *or* `reboot` system.

**Both** solution can work, but i recommend to start with **solution#1**, and if it changed nothing, apply **solution#2**. In my case i've used both solution, because i prefer curses as pinentry.

### mpi too large for this implementation
> gpg: mpi too large for this implementation (32768 bits)

> gpg: mpi too large for this implementation (46842 bits)

> gpg: keyring_get_keyblock: read error: invalid packet

> gpg: keydb_get_keyblock failed: invalid keyring

It happen when you try to read keys from keyring  with **vanilla** GnuPG(`gpg -K`/`gpg2 -K`) and keyring contain **large RSA** keys, generated in **modified GnuPG**.

**Solution:**

Install **modified** GnuPG with **modified** libgcrypt.


### keyring_get_keyblock/keydb_get_keyblock/Invalid packet

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

This happen when you generated large RSA key in **modified** GnuPG but in system you have **vanilla** libgcrypt.

**Solution:** install both modified.
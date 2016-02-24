# GnuPG with large RSA keys
## Description
GnuPG with large RSA keys (up to 32768 bytes).
This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website.

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
I've tested additionally on my side: RSA2048-OK, RSA4096-OK, RSA8192-OK, RSA16384-OK (19min), RSA32768-testing.

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


##This is devel branch, so it's under testing now.
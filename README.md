# GnuPG with large RSA keys
## Description
GnuPG with large RSA keys (up to 32768 bytes).
This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website.

## Compilation and intallation
### Debian
Download and execute from folder:
`./configure && make && make check`

If all tests are passed well, execute`checkinstall` and fill the fields like below:

> 1 -  Summary: [ gnupg2-2.0.29 ]
> 2 -  Name:    [ gnupg2 ]
> 3 -  Version: [ 2.0.29 ]
> 11 - Provides: [ gnupg2 ]

This version still undertesting.

This is devel branch, so it's under testing now.



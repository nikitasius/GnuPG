# GnuPG with large RSA keys
## Description
###GnuPG with large RSA keys (up to 32768 bytes).
## WARNING
## USE MODIFIED GNUPG AT YOUR OWN RISK. SOFTWARE MAY CAUSE DATA LOSS, SYSTEM CRASHES, AND RED EYES.
### Large keys, created in modified GnuPG with modified libgcrypt CANNOT be read by vanilla versions! It mean, that if you have another PGP stuff in your PC which work work with keys, you should to export keys OR create another keyring for vanilla versions.
This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website.

###[gnupg-2.0.29-RSA32k](https://github.com/nikitasius/GnuPG/tree/2.0.29-RSA32k)
This version based on GnuPG version [2.0.29](https://gnupg.org/ftp/gcrypt/gnupg/gnupg-2.0.29.tar.bz2), which can be downloaded from official https://gnupg.org website. To start i copied original GnuPG into the branch `gnupg-2.0.29`, where it stays **non**-modified. After it goes into `devel` branch where im working with. And after it goes into [2.0.29-RSA32k](https://github.com/nikitasius/GnuPG/tree/2.0.29-RSA32k) branch, where can be downloaded.

[**Download**](https://github.com/nikitasius/GnuPG/archive/2.0.29-RSA32k.zip) GnuPG-2.0.29 with RSA-32768

[**Download**](https://raw.githubusercontent.com/nikitasius/GnuPG/2.0.29-RSA32k/gnupg-2.0.29-RSA32k.patch) patch for vanilla gnupg-2.0.29 adding RSA-32768 support.


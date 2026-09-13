# Updater dependencies

This build uses curl 8.4.0, mbedTLS 2.28.8 and Jansson 2.14 with the official
devkitPro 3DS patches. Source URLs and SHA-256 digests are in sources.json.
The bundled Mozilla CA set comes from curl.se and is installed as
romfs:/update-ca.pem. Respective licenses are included here.

Build static ARM libraries with the devkitPro 3DS toolchain, then pass
-DMK64_3DS_UPDATE_DEPS=/path/to/prefix to CMake (or export MK64_3DS_UPDATE_DEPS
when using build.sh). mbedTLS enables hardware entropy and CMAC, disables
platform entropy, self-tests and timing; curl uses mbedTLS, HTTP/1.1 and
synchronous DNS, with IPv6, Unix sockets, threaded resolver, NTLM helper,
manual, pthreads, socketpair, LDAP and LDAPS disabled. Optional compression,
IDN, HTTP/2 and SSH libraries are excluded. Jansson is static without tests.

`platform/3ds/build.sh` builds these pinned sources automatically when the
build's updater prefix does not provide them. To prepare a separate private prefix:

```sh
python3 platform/3ds/build_update_deps.py /path/to/build/update-deps
MK64_3DS_UPDATE_DEPS=/path/to/build/update-deps/prefix ./platform/3ds/build.sh
```

The script verifies source hashes, applies the bundled patches and keeps all
third-party source/build products outside the tracked source files.

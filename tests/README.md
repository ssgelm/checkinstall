# checkinstall tests

Written while fixing the Debian package, and carried over here with the
patches they cover.

Everything runs unprivileged and installs nothing. The tests drive
checkinstall with `--fstrans=yes --install=no`, so writes land in a
translated tree under a temporary directory, and several of them assert that
the real filesystem was left alone.

## Running

    tests/run-tests.sh                            # all of them
    tests/run-tests.sh tests/tests/05-linkat.sh   # one
    CHECKINSTALL_SRC=/path/to/checkinstall tests/run-tests.sh
    CHECKINSTALL_DEB=/path/to/package.deb tests/run-tests.sh
    CHECKINSTALL_INSTALLED=1 tests/run-tests.sh

`CHECKINSTALL_SRC` defaults to the repository this lives in. The runner
builds that tree into a throwaway prefix and points the tests at it, so
whichever tree you name decides what is under test. To watch the tests fail,
name a checkout from before the fixes landed. Note that such a tree will not
build against glibc 2.33 or later at all, since the `_STAT_VER` definitions
arrived with these commits.

`CHECKINSTALL_INSTALLED=1` drives whatever is installed on this machine,
under /usr. Give it a prefix instead of `1` if the package used another one.
This is the mode for testing a distribution's package after installing it.
The rc file is looked for where upstream's Makefile puts it,
`PREFIX/lib/checkinstall/checkinstallrc`, except under /usr where
`/etc/checkinstallrc` is used if present. `CHECKINSTALL_RC` overrides both.

`CHECKINSTALL_DEB` unpacks a built package instead of building, and runs the
tests against the scripts and installwatch.so it ships. Both shipped scripts
take their prefix from the environment, so nothing has to be installed.
`01-upstream-suite.sh` still needs a source tree for test-installwatch.c.

Needs gcc, make, gettext (`msgfmt`) and dpkg-deb. `cmake` is optional and its
test skips without it. Four of the tests read the .deb that checkinstall
built, so they want dpkg-deb even though the rest do not.

**The checkout must not live under `/tmp` or `/var/tmp`.** installwatch
excludes both, so the tests would watch nothing. `run-tests.sh` refuses to
run from there.

## What each file covers

| test | covers |
| --- | --- |
| `01-upstream-suite.sh` | installwatch's own wrapper tests, both fstrans modes |
| `02-fstrans-ops.sh` | 19 everyday install operations under `--fstrans=yes` |
| `03-stat-and-mkdir.sh` | `mkdir -p`; `test -d`, `stat` and `ls` seeing what the install just made |
| `04-cmake.sh` | cmake's `file(INSTALL)`, which stats before copying |
| `05-linkat.sh` | hard links staying inside the translated tree; `AT_EMPTY_PATH` with an `O_TMPFILE` source |
| `06-tempfiles.sh` | `sed -i`; the LFS64 spellings `mkstemps64`/`mkostemps64` that 32-bit builds really call |
| `07-backup-symlinks.sh` | the backup tree mirroring real directories, and restoring it leaving `/lib` a symlink |
| `08-package-paths.sh` | no aliased `./lib` in the built package |
| `09-package-metadata.sh` | `--maintainer`, `--requires` and `--summary` surviving shell metacharacters |
| `10-addso.sh` | `--addso=yes` finding staged libraries, and its ld.so.conf guard holding under `/bin/sh` |
| `11-workdir-exclusion.sh` | the anchored build-directory exclusion, as a source check |
| `12-deb-compression.sh` | `--debcompression` reaching dpkg-deb, and a bad value stopping the run early |
| `13-filename-spaces.sh` | names and parent directories containing spaces, and a source directory with one |
| `14-symlink-modes.sh` | a mode set on a symlink staying off the file it points at |

`src/` holds two small C helpers: one that materialises an `O_TMPFILE`
through `linkat`, and one that calls the LFS64 temporary-file functions
directly.

## Writing another one

Tests are shell scripts that source `$HARNESS` and call `ok`, `fail` or
`skip`, then `finish`. The harness gives them `$WORK` (a scratch directory,
removed afterwards), `run_ck` to drive checkinstall and `run_iw` to run
something under installwatch alone. Numbering is only for ordering.

## Known gaps

- `11-workdir-exclusion.sh` checks the source rather than the behaviour.
  Driving that branch needs a build whose own files reach checkinstall's file
  list, which has not happened from this harness.
- Nothing here covers rpm or Slackware output, `--install=yes`, or the
  interactive prompts.
- `02` reports each operation separately but stops at the first one that
  breaks the chain, since later steps reuse earlier files.

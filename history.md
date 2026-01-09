# The sysv_ipc Module for Sys V IPC Under Python -- Version History

This is the version history for the [sysv_ipc module](https://github.com/osvenskan/sysv_ipc).

As of version 1.0.0, I consider this module complete. I will continue to support it and look for useful features to add, but right now I don't see any.

# Current/Latest – 1.2.0 (9 Jan 2026)

After five years, a new version! This is the ["I don't want to go on the cart"](https://www.youtube.com/watch?v=zEmfsmasjVA) release.

This release modernizes the file layout, building, and packaging of `sysv_ipc`, including many improvements copied from its sister project [`posix_ipc`](https://github.com/osvenskan/posix_ipc/). There are no changes to the core code other than the one behavior change noted below.

## Behavior Change

When attempting to set a Semaphore's value out of range (e.g. -1 or 99999), the module still raises a `ValueError`, but the associated message has changed. The previous message was "Attribute 'value' must be between 0 and 32767 (SEMAPHORE_VALUE_MAX)". The new message is "The semaphore's value must remain between 0 and SEMVMX"

## Deprecations

> [!IMPORTANT]
> The module constants `PAGE_SIZE` and `SEMAPHORE_VALUE_MAX` are deprecated as of this version. They will be removed in a future version. See https://github.com/osvenskan/sysv_ipc/issues/48 for background.

## Changes to System Discovery

 - Renamed `prober.py` to `discover_system_info.py`, which now raises `DiscoveryError` if it encounters a situation it can't handle.
 - Improved `does_build_succeed()` and `compile_and_run()`, including some ideas suggested by [Martin Jansa](https://github.com/shr-project) copied from https://github.com/osvenskan/posix_ipc/pull/77.
 - Added and expanded docstrings.
 - Changed to always write `SEMVMX` to `system_info.h`, and surround it with `#ifndef/#endif`.

## Other Changes

 - Converted all doc to Markdown
 - Integrated `cibuildwheel` to automate testing and create wheels for many platforms, thanks to [Matiiss](https://github.com/Matiiss).
 - Added `building.md` to document `discover_system_info.py` and the header file it creates.
 - Added a sample file (`system_info.sample.h`) that can be used as a template for writing your own.
 - Modernized the project to use `pyproject.toml` and modern build practices (e.g. `python -m build`). I reorganized the project's files to make it easier to use `pyproject.toml`.
 - Added `CONTRIBUTORS.txt` and updated copyright notices.
 - Added a GitHub issue template to make people more aware of the mailing list https://groups.io/g/python-sysv-ipc/

# Older Versions

## 1.1.0 (17 Jan 2021) —

 - Drop support for Python 2, also for Python 3.4 and 3.5.
 - **Behavior change:** `ftok()` now raises `OSError` if it fails so the particulars of the failure are visible to the caller. Thanks to James Williams for the suggestion. (Previously the function just returned -1 if it failed.)

## 1.0.1 (29 Nov 2019) —

 - Fix [a memory leak bug in `SharedMemory.write()` that only occurred under Python 3](https://github.com/osvenskan/sysv_ipc/issues/7). Thanks to Andy for reporting!
 - This will be the last version to support Python 2 which is pining for the fjords.

## 1.0.0 (2 Feb 2018) —

 - Added support for Python `memoryview` and `bytearray` objects to be created from a `SharedMemory` instance.
 - Added support for building wheels.
 - Moved source code hosting to git and GitHub.
 - Moved the demo code into its own directory.
 - Minor changes (slightly improved tests, a few compiler warnings fixed, PEP8 improvments, etc.)

## 0.7.0 (11 Feb 2016) —

Version 0.7.0 is finally out now that this package is 7 years old.

 - Dropped compatibility with Python < 2.7.
 - Added unit tests with almost complete coverage.
 - Fixed a doc bug for `SharedMemory.write()`. On an attempt to write outside of the segment, the code raises `ValueError`. The documentation had incorrectly stated that `write()` would silently discard the data that overflowed the segment.
 - Fixed a code bug in `SharedMemory.write()` that incorrectly interpreted offsets > `LONG_MAX`.
 - Fixed a code bug in `SharedMemory.write()` that didn’t always generate a `ValueError` for negative offsets.
 - Fixed a doc/code bug where `SharedMemory.attach()` was documented to accept keyword arguments but did not. Now it does.
 - Fixed some header bugs that prevented the code from using `PY_SSIZE_T_CLEAN`. Thanks to Tilman Koschnick for bringing this to my attention.
 - Added doc to note that this module exposes `SHM_RDONLY` as a module-level constant.

## 0.6.8 (12 Sept 2014) —

 - Fixed a bug in `prober.py` where prober would fail on systems where Python's include file was not named `pyconfig.h`. (RHEL 6 is one such system.) Thanks to Joshua Harlow for the bug report and patch.
 - *Spasibo* again to Yuriy Taraday for pointing out that the semaphore initialization example I changed in the previous version was still not quite correct, and for suggesting something better.

## 0.6.7 (1 August 2014) —

*Spasibo* to Yuriy Taraday for reporting some doc errors corrected in this version.

 - Added `KEY_MIN` as a module-level constant. The documentation has claimed that it was available for a long time so now the code finally matches the documentation.
 - Changed randomly generated keys to never use a value of 0.
 - Fixed two documenation errors in the special section on semaphore initialization and gave long overdue credit to W. Richard Stevens' book for the code idea. (Any code mistakes are mine, not his.)
 - This is the first version downloadable from PyPI.

## 0.6.6 (15 October 2013) —

 - Added the ability to use Semaphores in context managers. Thanks to Matt Ruffalo for the suggestion and patch.

## 0.6.5 (31 March 2013) —

 - Fixed a bug where `SharedMemory.write()` claimed to accept keyword arguments but didn't actually do so. Thanks to Matt Ruffalo for the bug report and patch.

## 0.6.4 (19 Dec 2012) —

 - Added a module-level `attach()` method based on a suggestion by Vladimír Včelák.
 - Added the `ftok()` method along with dire warnings about its use.

## 0.6.3 (3 Oct 2010) —

 - Fixed a segfault in `write()` that occurred any time an offset was passed. This was introduced in v0.6.0. *Tack* to Johan Bernhardtson for the bug report.

## 0.6.2 (6 July 2010) —

 - Updated `setup.py` metadata to reflect Python 3 compatibility. No functional change.

## 0.6.1 (26 June 2010) —

 - Fixed a typo introduced in the previous version that caused unpredictable behavior (usually a segfault) if a block flag was passed to `MessageQueue.send()`. *Obrigado* to Álvaro Justen and Ronald Kaiser for the bug report and patch.

## 0.6.0 (20 May 2010) —

 - Added Python 3 support.
 - Renamed a constant that caused a problem under AIX. Thanks to Loic Nageleisen for the bug report.
 - Updated this documentation a little.

## 0.5.2 (17 Jan 2010) —

 - Fixed a bug that insisted on keys > 0. Thanks to 原志 (Daniel) for the bug report and patch.
 - Fixed a bug where the module could have generated invalid keys on systems that typedef `key_t` as a `short`. I don't think such a system exists, though.
 - Fixed a LICENSE file typo.
 - Added an RSS feed to `history.html`.

## 0.5.1 (1 Dec 2009) —

No code changes in this version.

 - Fixed the comment in `sysv_ipc_module.c` that still referred to the GPL license.
 - Added the attributes `__version`, `__author__`, `__license__` and `__copyright__`.
 - Removed `file()` from `setup.py` in favor of `open()`.

## 0.5.0 (6 Oct 2009) —

No code changes in this version.

 - Changed the license from GPL to BSD.

## 0.4.2 (22 Mar 2009) —

No code changes in this version.

 - Fixed broken documentation links to youtube.
 - Fixed the project name in the LICENSE file.

## 0.4.1 (12 Feb 2009) —

 - Changed status to beta.
 - Added automatic generation of keys.
 - Added a message queue demo.
 - Added `str()` and `repr()` support to all objects.
 - Fixed a bug in `SharedMemory.write()` that could cause a spurious error of "Attempt to write past end of memory segment". This bug only occurred on platforms using different sizes for `long` and `int`, or `long` and `py_size_t` (depending on Python version). *Tack* to Jesper for debug help.
 - Plugged a memory leak in `MessageQueue.receive()`.
 - Added a VERSION attribute to the module.

## 0.4.0 (28 Jan 2009) —

 - Added message queues.
 - Fixed a bug where the `key` attribute of SharedMemory objects returned garbage.
 - Fixed a bug where keys &gt; INT_MAX would get truncated on platforms where longs are bigger than ints.
 - Provided decent inline documentation for object attributes (available via the `help()` command).
 - Rearranged the code which was suffering growing pains.

## 0.3.0 (16 Jan 2009) —

This version features a rename of classes and errors (sorry about breaking the names), some modifications to semaphore timeouts, and many small fixes.

A semaphore's `.block` flag now consistently trumps the timeout. When `False`, the timeout is irrelevant -- calls will never block. In prior versions, the flag was ignored when the timeout was non-zero.

Also, on platforms (such as OS X) where `semtimedop()` is not supported, all timeouts are now treated as `None`. In other words, when `.block` is True, all calls wait as long as necessary.

Other fixes —

 - Fixed the poor choices I'd made for class and error names by removing the leading "SysV" and "SysVIpc".
 - Removed dependencies on Python 2.5. The module now works with Python 2.4.4 and perhaps earlier versions.
 - Fixed a bug where I was not following SysV semaphore semantics for interaction between timeouts and the block flag.
 - Fixed compile problems on OpenSolaris 2008.05.
 - Got rid of OS X compile warnings.
 - Fixed many instances where I was making potentially lossy conversions between Python values and Unix-specific types like `key_t`, `pid_t`, etc.
 - Fixed a bug in the SharedMemory `attach()` function that would set an error string but not return an error value if the passed address was not `None` or a long.
 - Simplified the code a little.
 - Restricted the semaphore `acquire()` and `release()` delta to be between `SHORT_MIN` and `SHORT_MAX` since [it is a short in the specification](https://pubs.opengroup.org/onlinepubs/9799919799/functions/semop.html)
 - Fixed a bug where calling `acquire()` or `release()` with a delta of `0` would call `.Z()` instead.
 - Disallowed byte counts ≤ `0` in `SharedMemory.read()`
 - Fixed a bug in the `SharedMemory` init code that could, under vanishingly rare circumstances, return failure without setting the error code.
 - Removed some dead code relating to the `.seq` member of the `sem_perm` and `shm_perm` structs.
 - Stopped accessing the non-standard `key` (a.k.a. `_key` and `__key`) element of the `ipc_perm` struct.
 - Added code to ensure the module doesn't try to create a string that's larger than Python permits when reading from shared memory.

## 0.2.1 (3 Jan 2009) —

 - Fixed a bug that prevented the module-specific errors (`ExistentialError`, etc.) from being visible in the module.
 - Fixed a bug that re-initialized shared memory with the init character when only `IPC_CREAT` was specified and an existing segment was opened.
 - Fixed a bug that always defaulted the size of a shared memory segment to `PAGE_SIZE`. Updated code and documentation to use intelligent defaults. *Tack* to Jesper for the bug report.
 - Several cosmetic changes. (Added more metadata to `setup.py`, added a newline to the end of `probe_results.h` to avoid compiler complaints, etc.)

## 0.2.0 (16 Dec 2008) —

Lots of small fixes.

 - Fixed a bug where calling `shm.remove()` on shared memory that was already removed would cause a SystemError. (I wasn't setting the Python error before returning.)
 - Fixed a couple of bugs that would cause the creation of a new, read-only shared memory segment to fail.
 - Fixed a bug that would cause the creation of a new, read-only semaphore to fail.
 - Added the constant `IPC_CREX`.
 - Renamed (sorry) `MAX_KEY` to `KEY_MAX` and `MAX_SEMAPHORE_VALUE` to `SEMAPHORE_VALUE_MAX` to be consistent with the C naming convention in limits.h.
 - Hardcoded `SEMAPHORE_VALUE_MAX` to 32767 until I can find a reliable way to determine it at install time.
 - Changed `prober.py` to write out a C header file with platform-specific definitions in it.
 - Replaced `OSError` in shared memory functions with custom errors from this module.
 - Added code to raise a `ValueError` when an attempt is made to assign an out-of-range value to a semaphore.
 - Added code to raise a `ValueError` when an out-of-range key is passed to a constructor.
 - Fixed a bug in the demo program `conclusion.c` that caused some informational messages to be printed twice.
 - Fixed some documentation bugs.

## 0.1.0 (4 Dec 2008) —

 - Original (alpha) version.

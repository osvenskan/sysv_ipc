import subprocess
import os
import shlex

# Set these to None for compile/link debugging or subprocess.PIPE to silence
# compiler warnings and errors.
STDOUT = subprocess.PIPE
STDERR = subprocess.PIPE
# STDOUT = None
# STDERR = None

# OUTPUT_FILEPATH is the file that this script will write, if necessary. The path is relative
# to the project root. Setuptools guarantees that the project root will be the working directory
# when this script executes.
OUTPUT_FILEPATH = "./src/system_info.h"

class DiscoveryError(Exception):
    '''Exception raised when this script is unable to discover a value that it needs.'''
    pass


def _does_build_succeed(filename):
    '''Returns True if the file compiles and links successfully, False otherwise.

    It can be perfectly normal for the build to fail, e.g. when building discover_semtimedop.c on
    a system where sem_timedop() doesn't exist.

    Note that unlike compile_and_run(), this just builds an executable. It does not attempt to
    run that executable.
    '''
    cc = os.getenv("CC", "cc")
    cmd = [
       *shlex.split(cc),
       '-Wall',
       '-o',
       f'./build_support/src/{filename[:-2]}',
       f'./build_support/src/{filename}',
       ]
    p = subprocess.Popen(cmd, stdout=STDOUT, stderr=STDERR)

    # p.wait() returns the process' return code, so 0 implies that the compile & link succeeded.
    return not bool(p.wait())


def _compile_and_run(filename):
    '''Compiles and links the file, runs the executable, and returns whatever the executable
    prints to stdout.

    Failure of any of the steps (compile, link, run) is unexepected. This function returns None
    in that case.
    '''
    if _does_build_succeed(filename):
        cmd = f"./build_support/src/{filename[:-2]}"
        try:
            s = subprocess.Popen([cmd], stdout=subprocess.PIPE).communicate()[0]
            return s.strip().decode()
        except Exception:
            # Execution resulted in an error. This is unexpected.
            return None
    else:
        # Build resulted in an error. This is unexpected.
         return None


def _maybe_get_sysconf_value(name):
    """Returns the value of a sysconf entry (e.g. 'SC_PAGESIZE') if the entry exists. If the value
    isn't available via sysconf, this function returns None.
    """
    value = None

    # Years ago, Cygwin didn't support os.sysconf_names. That has probably changed, but I don't
    # want to assume.
    if hasattr(os, 'sysconf_names'):
        if name in os.sysconf_names:
            value = os.sysconf(name)

    return value


def _discover_semtimedop():
    '''Returns True if the host system supports semtimedop(), False otherwise.'''
    return _does_build_succeed("discover_semtimedop.c")


def _discover_semun_union_defined():
    '''Returns True if the semun union is defined in a system header file, False otherwise.'''
    return _does_build_succeed("discover_semun_union_defined.c")


def _discover_page_size():
    '''Returns the page size (usually 4096 or 16384) suitable for inclusion in system_info.h.

    Raises a DiscoveryError exception if unable to determine the page size.
    '''
    page_size = None

    # When cross compiling under cibuildwheel, I need to rely on their custom env var to set the
    # page size correctly. See https://github.com/osvenskan/posix_ipc/issues/58
    if 'arm' in os.getenv('_PYTHON_HOST_PLATFORM', ''):
        page_size = 16384

    if not page_size:
        # Page size is usually present in sysconf(), and checking sysconf is easier than invoking
        # the compiler.
        page_size = _maybe_get_sysconf_value('SC_PAGESIZE')

    if not page_size:
        # OK, I have to do it the hard way.
        page_size = _compile_and_run("discover_page_size.c")

    if not page_size:
        raise DiscoveryError('Unable to determine page size')

    return page_size


def discover():
    '''This is the main entry point for this script. It returns a dict of information it has
    discovered about the buld system. If system_info.h already exists when this script is
    invoked, then the returned dict is empty.

    If system_info.h does not exist, this script creates and populates it, and the values in the
    returned dict are what it wrote to system_info.h.
    '''
    sys_info = {"KEY_MAX": "LONG_MAX", "KEY_MIN": "LONG_MIN"}

    # conditionals contains preprocessor #defines to be written to system_info.h that might
    # already be defined on some platforms. Any symbol in this list will be surrounded with
    # preprocessor directives #ifndef/#endif in system_info.h.
    # If a symbol is in this list but isn't written to system_info.h, no harm done.
    CONDITIONALS = ("_SEM_SEMUN_UNDEFINED",
                    # PAGE_SIZE is already #defined elsewhere on FreeBSD.
                    "PAGE_SIZE",
                    # SEMVMX is often #defined in a system header file
                    "SEMVMX",
                    )

    if os.path.exists(OUTPUT_FILEPATH):
        # As guaranteed in building.md, this script is (mostly) a no-op if the output file already
        # exists.
        pass
    else:
        # system_info.h doesn't exist, so I need to figure out what values should go into it, and
        # then write the file.

        # Version info is easy. :-)
        with open("VERSION") as f:
            sys_info["SYSV_IPC_VERSION"] = f'"{f.read().strip()}"'

        sys_info["PAGE_SIZE"] = _discover_page_size()

        if _discover_semtimedop():
            sys_info["SEMTIMEDOP_EXISTS"] = ""

        # I hardcode the max value of a sempahore. I expect that this value is fine for most
        # users, and those that need something different can use their own system_info.h.
        # Details: https://github.com/osvenskan/sysv_ipc/issues/3
        sys_info["SEMVMX"] = 32767

        # On some platforms, it's the responsibility of my code to define the semun union, on
        # other platforms, it's already defined in a system header file. To avoid potential
        # trouble, I detect when it is already defined and don't attempt to define it myself.

        # For instance, on the Mac, sem.h says, "The union definition is intended to be defined by
        # the user application...it is provided here" 🙃 The comment goes on to give reasons,
        # including "historical source compatability".
        if not _discover_semun_union_defined():
            sys_info["_SEM_SEMUN_UNDEFINED"] = ""

        # Turn each of the values in sys_info into lines that will be written to system_info.h
        lines = []

        for key in sys_info:
            if key in CONDITIONALS:
                lines.append(f"#ifndef {key}")

            lines.append(f"#define {key}\t\t{sys_info[key]}")

            if key in CONDITIONALS:
                lines.append("#endif")

        # A trailing blank line keeps compilers happy.
        lines.append('')

        msg = """/*
This header file was generated by discover_system_info.py. You can delete it,
edit it, or even write your own. See building.md for details.
*/

"""
        with open(OUTPUT_FILEPATH, 'w') as f:
            f.write(msg + '\n'.join(lines))

    return sys_info


if __name__ == "__main__":
    print(discover())

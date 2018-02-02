import os.path
import os
import subprocess
import distutils.sysconfig

# Set these to None for debugging or subprocess.PIPE to silence compiler
# warnings and errors.
STDOUT = subprocess.PIPE
STDERR = subprocess.PIPE
# STDOUT = None
# STDERR = None

# This is the max length that I want a printed line to be.
MAX_LINE_LENGTH = 78

PYTHON_INCLUDE_DIR = os.path.dirname(distutils.sysconfig.get_config_h_filename())
# print(PYTHON_INCLUDE_DIR)


def line_wrap_paragraph(s):
    # Format s with terminal-friendly line wraps.
    done = False
    beginning = 0
    end = MAX_LINE_LENGTH - 1
    lines = []
    while not done:
        if end >= len(s):
            done = True
            lines.append(s[beginning:])
        else:
            last_space = s[beginning:end].rfind(' ')

            lines.append(s[beginning:beginning + last_space])
            beginning += (last_space + 1)
            end = beginning + MAX_LINE_LENGTH - 1

    return lines


def print_bad_news(value_name, default):
    s = "Setup can't determine %s on your system, so it will default to %s which may not " + \
        "be correct."
    s = s % (value_name, default)

    plea = "Please report this message and your operating system info to the package " + \
           "maintainer listed in the README file."

    lines = line_wrap_paragraph(s) + [''] + line_wrap_paragraph(plea)

    border = '*' * MAX_LINE_LENGTH

    s = border + "\n* " + ('\n* '.join(lines)) + '\n' + border

    print(s)


def does_build_succeed(filename):
    # Utility function that returns True if the file compiles and links
    # successfully, False otherwise.
    cmd = "cc -Wall -I%s -o ./prober/foo ./prober/%s" %  \
                (PYTHON_INCLUDE_DIR, filename)

    p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)

    # p.wait() returns the process' return code, so 0 implies that
    # the compile & link succeeded.
    return not bool(p.wait())


def compile_and_run(filename, linker_options=""):
    # Utility function that returns the stdout output from running the
    # compiled source file; None if the compile fails.
    cmd = "cc -Wall -I%s -o ./prober/foo %s ./prober/%s" %  \
                (PYTHON_INCLUDE_DIR, linker_options, filename)

    p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)

    if p.wait():
        # uh-oh, compile failed
        return None
    else:
        s = subprocess.Popen(["./prober/foo"],
                             stdout=subprocess.PIPE).communicate()[0]
        return s.strip().decode()


def sniff_semtimedop():
    return does_build_succeed("semtimedop_test.c")


def sniff_union_semun_defined():
    # AFAICT the semun union is supposed to be declared in one's code.
    # However, a lot of legacy code gets this wrong and some header files
    # define it, e.g.sys/sem.h on OS X where it's #ifdef-ed so that legacy
    # code won't break. On some systems, it appears and disappears based
    # on the #define value of _XOPEN_SOURCE.
    return does_build_succeed("sniff_union_semun_defined.c")


def probe_semvmx():
    # At present, this is hardcoded and that seems fine on all systems I've tested.
    # https://github.com/osvenskan/sysv_ipc/issues/3
    semvmx = 32767

    return semvmx


def probe_page_size():
    DEFAULT_PAGE_SIZE = 4096

    page_size = compile_and_run("probe_page_size.c")

    if page_size is None:
        page_size = DEFAULT_PAGE_SIZE
        print_bad_news("the value of PAGE_SIZE", page_size)

    return page_size


def probe():
    d = {"KEY_MAX": "LONG_MAX",
         "KEY_MIN": "LONG_MIN"
         }

    # conditionals contains preprocessor #defines to be written to probe_results.h that might
    # already be defined on some platforms. Any symbol in this list will be surrounded with
    # preprocessor directives #ifndef/#endif in probe_results.h.
    # If a symbol is in this list but isn't written to probe_results.h, no harm done.
    conditionals = ["_SEM_SEMUN_UNDEFINED",
                    # PAGE_SIZE is already #defined elsewhere on FreeBSD.
                    "PAGE_SIZE",
                    ]

    version = open("VERSION").read().strip()

    d["SYSV_IPC_VERSION"] = '"%s"' % version
    d["PAGE_SIZE"] = probe_page_size()
    if sniff_semtimedop():
        d["SEMTIMEDOP_EXISTS"] = ""
    d["SEMAPHORE_VALUE_MAX"] = probe_semvmx()
    # Some (all?) Linux platforms #define _SEM_SEMUN_UNDEFINED if it's up
    # to my code to declare this union, so I use that flag as my standard.
    if not sniff_union_semun_defined():
        d["_SEM_SEMUN_UNDEFINED"] = ""

    msg = """/*
This header file was generated when you ran setup. Once created, the setup
process won't overwrite it, so you can adjust the values by hand and
recompile if you need to.

To enable lots of debug output, add this line and re-run setup.py:
#define SYSV_IPC_DEBUG

To recreate this file, just delete it and re-run setup.py.

KEY_MIN, KEY_MAX and SEMAPHORE_VALUE_MAX are stored internally in longs, so
you should never #define them to anything larger than LONG_MAX regardless of
what your operating system is capable of.

*/

"""

    filename = "probe_results.h"
    if not os.path.exists(filename):
        lines = []

        for key in d:
            if key in conditionals:
                lines.append("#ifndef %s" % key)

            lines.append("#define %s\t\t%s" % (key, d[key]))

            if key in conditionals:
                lines.append("#endif")

        # A trailing '\n' keeps compilers happy...
        with open(filename, "w") as f:
            f.write(msg + '\n'.join(lines) + '\n')

    return d


if __name__ == "__main__":
    s = probe()
    print(s)

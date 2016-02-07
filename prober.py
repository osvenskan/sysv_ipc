import os.path
import os
import subprocess
import sys
import distutils.sysconfig

# Set these to None for debugging or subprocess.PIPE to silence compiler
# warnings and errors.
STDOUT = subprocess.PIPE
STDERR = subprocess.PIPE
# STDOUT = None
# STDERR = None

# This is the max length that I want a printed line to be.
MAX_LINE_LENGTH = 78

PYTHON_INCLUDE_DIR = distutils.sysconfig.get_config_h_filename()[:-len("/pyconfig.h")]
#print PYTHON_INCLUDE_DIR

def line_wrap_paragraph(s):
    # Format s with terminal-friendly line wraps.
    done = False
    beginning = 0
    end = MAX_LINE_LENGTH - 1
    lines = [ ]
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
    s = "Setup can't determine %s on your system, so it will default to %s which may not be correct." \
            % (value_name, default)
    plea = "Please report this message and your operating system info to the package maintainer listed in the README file."
    
    lines = line_wrap_paragraph(s) + [''] + line_wrap_paragraph(plea)

    border = '*' * MAX_LINE_LENGTH
    
    s = border + "\n* " + ('\n* '.join(lines)) + '\n' + border
    
    print (s)


def does_build_succeed(filename):
    # Utility function that returns True if the file compiles and links
    # successfully, False otherwise.    
    cmd = "cc -Wall -I%s -o ./prober/foo ./prober/%s" %  \
                (PYTHON_INCLUDE_DIR, filename)

    p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)
        
    # p.wait() returns the process' return code, so 0 implies that 
    # the compile & link succeeded.
    return not bool(p.wait())

def compile_and_run(filename, linker_options = ""):
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
    # This is the hardcoded default that I chose for two reasons. First, 
    # it's the default on my Mac so I know at least one system needs it 
    # this low. Second, it fits neatly into a 16-bit signed int which 
    # makes me hope that it's low enough to be safe on all systems.
    semvmx = 32767
    
    # FIXME -- Ways to get SEMVMX -- 
    # 1) Try to compile .c code assuming SEMVMX is #defined 
    # 2) Parse the output from `sysctl kern.sysv.semvmx` (doesn't work 
    #    on OS X).
    # 3) Parse the output from `ipcs -S`
    # 4) Run .c code that loops, releasing a semaphore until semop() 
    #    returns ERANGE or the value goes wonky. OS X lets the latter
    #    happen, which AFAICT is a violation of the spec.
    
    return semvmx
    


def probe_page_size():
    DEFAULT_PAGE_SIZE = 4096
    
    page_size = compile_and_run("probe_page_size.c")
    
    if page_size is None:
        page_size = DEFAULT_PAGE_SIZE
        print_bad_news("the value of PAGE_SIZE", page_size)

    return page_size


def probe():
    d = { "KEY_MAX" : "LONG_MAX", 
          "KEY_MIN" : "LONG_MIN" 
        }

    conditionals = [ "_SEM_SEMUN_UNDEFINED" ]
    
    f = open("VERSION")
    version = f.read().strip()
    f.close()

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

To recreate this file, just delete it and re-run setup.py.

KEY_MIN, KEY_MAX and SEMAPHORE_VALUE_MAX are stored internally in longs, so 
you should never #define them to anything larger than LONG_MAX.
*/

"""
    
    filename = "probe_results.h"
    if not os.path.exists(filename):
        lines = [ ]
        
        for key in d:
            if key in conditionals:
                lines.append("#ifndef %s" % key)

            lines.append("#define %s\t\t%s" % (key, d[key]))

            if key in conditionals:
                lines.append("#endif")

        # A trailing '\n' keeps compilers happy...
        f = open(filename, "w")
        f.write(msg + '\n'.join(lines) + '\n')
        f.close()

    return d

if __name__ == "__main__":
    s = probe()
    print (s)

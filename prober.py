import os.path
import os
import subprocess
import sys

# Set these to None for debugging or subprocess.PIPE to silence compiler
# warnings and errors.
STDOUT = subprocess.PIPE
STDERR = subprocess.PIPE
# STDOUT = None
# STDERR = None


# This module makes use of the ipc_perm structure defined in <sys/ipc.h>. 
# That structure includes an element called key, _key or __key depending on 
# the OS. Here's the variations I've found:
#     key: FreeBSD 6, AIX, Solaris, OS X 10.3, OS X 10.4+ (conditionally)
#    _key: NetBSD, OS X 10.4+ (conditionally)
#   __key: Debian 4 (Etch), Ubuntu 7, RH 3.3 & 3.4

# OS X side note: OS X >= 10.4's ipc.h (see 
# /Developer/SDKs/MacOSX10.4u.sdk/usr/include/sys/ipc.h) defines structs 
# called __ipc_perm_old (which uses key) and __ipc_perm_new (which uses _key). 
# The latter is used if _XOPEN_SOURCE is #defined to 600.

# Trying to enumerate all of the variations on all platforms is a fool's 
# errand so instead I figure it out on the fly using the function 
# count_key_underscores() below.

# Lots of systems use the unadorned "key" so that's the default when I'm 
# forced to guess.
COUNT_KEY_UNDERSCORES_DEFAULT = 0

def count_key_underscores():
    # Here I compile three mini-programs with key, _key and __key. Theoretically, 
    # two should fail and one should succeed, and that will tell me how this platform names
    # ipc_perm.key. If the number of successes != 1, something's gone wrong.
    # I use subprocess in order to trap (and discard) stderr so that the user doesn't 
    # see the compiler errors I'm deliberately generating here.
    underscores_count = COUNT_KEY_UNDERSCORES_DEFAULT
    
    underscores = { }

    src = """
#define _XOPEN_SOURCE 600
#include <sys/ipc.h>
int main(void) { struct ipc_perm foo; foo.%skey = 42; }

"""
    for i in range(0, 3):
        source_filename = "./prober/%d.c" % i
        file(source_filename, "w").write(src % ('_' * i))
        
        cmd = "cc -o ./prober/foo " + source_filename
        
        p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)
        if not p.wait(): underscores[i] = True
        
    key_count = len(underscores.keys())

    if key_count == 1:
        underscores_count = underscores.keys()[0]
    else:
        print """
*********************************************************************
* I was unable to detect the structure of ipc_perm on your system.  *
* I'll make my best guess, but compiling might fail anyway. Please  *
* email this message, the error code of %d, and the name of your OS  *
* to the contact at http://semanchuk.com/philip/sysv_ipc/.         *
*********************************************************************
""" % key_count
        
    return underscores_count



COUNT_SEQ_UNDERSCORES_DEFAULT = 0

def count_seq_underscores():
    """ Uses trial-and-error with the system's C compiler to figure out the number of 
        underscores preceding key in the ipc_perm structure. Returns 0, 1 or 2. In case of
        error, it makes a guess and hopes for the best.
    """
    underscores_count = COUNT_SEQ_UNDERSCORES_DEFAULT
    
    underscores = { }

    src = """
#define _XOPEN_SOURCE 600
#include <sys/ipc.h>
int main(void) { struct ipc_perm foo; foo.%sseq = 42; }

"""
    for i in range(0, 3):
        source_filename = "./prober/%d.c" % i
        file(source_filename, "w").write(src % ('_' * i))
        
        cmd = "cc -o ./prober/foo " + source_filename
        
        p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)
        if not p.wait(): underscores[i] = True
        
    key_count = len(underscores.keys())

    if key_count == 1:
        underscores_count = underscores.keys()[0]
    else:
        print """
*********************************************************************
* I was unable to detect the structure of ipc_perm on your system.  *
* I'll make my best guess, but compiling might fail anyway. Please  *
* email this message, the error code of %d, and the name of your OS  *
* to the contact at http://semanchuk.com/philip/sysv_ipc/.          *
*********************************************************************
""" % key_count
        
    return underscores_count

 

def sniff_semtimedop():
    rc = False

    cmd = "cc -o ./prober/foo ./prober/semtimedop_test.c"
    
    p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)
    if not p.wait(): rc = True

    return rc


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
    


def probe():
    d = { }

    # AFAICT the semun union is supposed to be declared in one's code. 
    # However, a lot of legacy code gets this wrong and some header files 
    # define it, e.g.sem.h on OS X where it's #ifdef-ed so that legacy code 
    # won't break. In fact it's my #define of _XOPEN_SOURCE that causes it 
    # to not exist on darwin. Notably, some (all?) Linux platforms #define 
    # _SEM_SEMUN_UNDEFINED if it's up to my code to declare this union, so 
    # I use that flag as my standard.
    if "darwin" in sys.platform:
        d["_SEM_SEMUN_UNDEFINED"] = ""
    
    d["IPC_PERM_KEY_NAME"] = ("_" * count_key_underscores()) + "key"
    d["IPC_PERM_SEQ_NAME"] = ("_" * count_seq_underscores()) + "seq"
    if sniff_semtimedop():
        d["SEMTIMEDOP_EXISTS"] = ""
    d["UID_MAX"] = "INT_MAX"
    d["GID_MAX"] = "INT_MAX"
    d["KEY_MAX"] = "INT_MAX"
    d["SEMAPHORE_VALUE_MAX"] = probe_semvmx()
    d["PY_INT_MAX"] = sys.maxint
    
    #print d

    msg = """/*
This header file was generated when you ran setup. Once created, the setup
process won't overwrite it, so you can adjust the values by hand and 
recompile if you need to. 

To recreate this file, just delete it and re-run setup.py.

Note that UID_MAX, GID_MAX, KEY_MAX and SEMAPHORE_VALUE_MAX are stored
internally in longs, so you should never #define them to anything 
larger than LONG_MAX on your platform.
*/

"""
    
    filename = "probe_results.h"
    if not os.path.exists(filename):
        lines = ["#define %s\t\t%s\n" % (key, d[key]) for key in d]
        file(filename, "wb").write(msg + ''.join(lines))


if __name__ == "__main__":
    probe()

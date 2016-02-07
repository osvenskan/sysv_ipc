import os.path
import os
import subprocess

# Test compiles write their output to a folder called "junk" because 
# I can't write to /dev/null under OS X.

# Set these to None for compiler debugging or subprocess.PIPE to silence errors.
STDOUT = subprocess.PIPE
STDERR = subprocess.PIPE
# STDOUT = None
# STDERR = None



# This module makes use of the ipc_perm structure defined in include/sys/ipc.h
# (or include/bits/ipc.h on some systems). That structure includes an element 
# called key, _key or __key depending on the OS. Here's the variations I've 
# been able to find with OS version numbers where possible. 
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

# Lots of systems use the unadorned "key" so that's the default when I'm forced to guess.
COUNT_KEY_UNDERSCORES_DEFAULT = 0

key_underscores = COUNT_KEY_UNDERSCORES_DEFAULT

def count_key_underscores():
    """ Uses trial-and-error with the system's C compiler to figure out the number of 
        underscores preceding key in the ipc_perm structure. Returns 0, 1 or 2. In case of
        error, it makes a guess and hopes for the best.
    """
    underscores_count = COUNT_KEY_UNDERSCORES_DEFAULT
    
    underscores = { }

    # Here I compile three mini-programs with key, _key and __key. Theoretically, 
    # two should fail and one should succeed, and that will tell me how this platform names
    # ipc_perm.key. If the number of successes != 1, something's gone wrong.
    # I use subprocess in order to trap (and discard) stderr so that the user doesn't 
    # see the compiler errors I'm deliberately generating here.
    src = """
#define _XOPEN_SOURCE 600
#include <sys/ipc.h>
int main(void) { struct ipc_perm foo; foo.%skey = 42; }

"""
    for i in range(0, 3):
        source_filename = "./junk/%d.c" % i
        file(source_filename, "w").write(src % ('_' * i))
        
        cmd = "cc -o ./junk/foo " + source_filename
        
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

seq_underscores = COUNT_KEY_UNDERSCORES_DEFAULT

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
        source_filename = "./junk/%d.c" % i
        file(source_filename, "w").write(src % ('_' * i))
        
        cmd = "cc -o ./junk/foo " + source_filename
        
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
    
    cmd = '''printf "#define _XOPEN_SOURCE 600\n#include <sys/sem.h>\n#include <stdlib.h>\nint main(void) { semtimedop(0, NULL, 0, NULL); }\n" | cc -xc -o ./junk/foo -'''
    p = subprocess.Popen(cmd, shell=True, stdout=STDOUT, stderr=STDERR)
    if not p.wait(): 
        rc = True

    return rc


def probe():
    global key_underscores
    global seq_underscores
    global semtimedop_available

    
    if not os.path.exists("./junk"):
        os.mkdir("./junk")
    
    key_underscores = count_key_underscores()
    seq_underscores = count_seq_underscores()
    semtimedop_available = sniff_semtimedop()
    

if __name__ == "__main__":
    probe()
    
    print "key underscores = %d " % key_underscores
    print "seq underscores = %d " % seq_underscores
    print "semtimedop_available = %d " % semtimedop_available


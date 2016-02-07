import tarfile
import os
import os.path
import re

VERSION = file("VERSION").read().strip()

filenames = (
    "LICENSE",
    "INSTALL",
    "README",
    "VERSION",
    "ReadMe.html",
    "setup.py",
    "sysv_ipc_module.c",
    "demo/",
    "demo/cleanup.py",
    "demo/conclusion.c",
    "demo/conclusion.py",
    "demo/md5.c",
    "demo/md5.h",
    "demo/mk.sh",
    "demo/params.txt",
    "demo/premise.c",
    "demo/premise.py",
    "demo/utils.c",
    "demo/utils.h",
    "demo/utils.py",
    "prober.py",
    "prober/",
    "prober/semtimedop_test.c"
)

tarball_name = "sysv_ipc-%s.tar.gz" % VERSION

if os.path.exists(tarball_name):
    os.remove(tarball_name)

SourceDir = "./"
BundleDir = "sysv_ipc-%s/" % VERSION

tarball = tarfile.open("./" + tarball_name, "w:gz")
for name in filenames:
    SourceName = SourceDir + name
    BundledName = BundleDir + name

    print "Adding " + SourceName

    tarball.add(SourceName, BundledName, False)
tarball.close()

# # Check to see if I've left the DEBUG flag enabled.
# s = file("sysv_ipc_module.c").read()
# 
# if not re.search("""\s*//\s*#define\s+SYSV_IPC_DEBUG""", s):
#     print """
# ******************************************************
#   Are you sure that SYSV_IPC_DEBUG is commented out?
# ******************************************************
# """



import tarfile
import os
import os.path
import hashlib

VERSION = file("VERSION").read().strip()

filenames = (
#    "memory_leak_tests.py",
    "LICENSE",
    "INSTALL",
    "README",
    "VERSION",
    "ReadMe.html",
    "setup.py",
    "sysv_ipc_module.c",
    "common.c",
    "common.h",
    "mq.c",
    "mq.h",
    "semaphore.c",
    "semaphore.h",
    "memory.c",
    "memory.h",
    "demo/",
    "demo/ReadMe.txt",
    "demo/cleanup.py",
    "demo/conclusion.c",
    "demo/conclusion.py",
    "demo/md5.c",
    "demo/md5.h",
    "demo/make_all.sh",
    "demo/params.txt",
    "demo/premise.c",
    "demo/premise.py",
    "demo/utils.c",
    "demo/utils.h",
    "demo/utils.py",
    "demo/SampleIpcConversation.png",
    "demo2/",
    "demo2/ReadMe.txt",
    "demo2/cleanup.py",
    "demo2/conclusion.py",
    "demo2/params.txt",
    "demo2/premise.py",
    "demo2/utils.py",
    "demo2/SampleIpcConversation.png",
    "prober.py",
    "prober/",
    "prober/semtimedop_test.c",
    "prober/probe_page_size.c",
    "prober/sniff_union_semun_defined.c",
)

tarball_name = "sysv_ipc-%s.tar.gz" % VERSION
md5_name = "sysv_ipc-%s.md5.txt" % VERSION

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

s = file("./" + tarball_name).read()

s = hashlib.md5(s).hexdigest()

print "md5 = " + s

file(md5_name, "w").write(s)



# # Check to see if I've left the DEBUG flag enabled.
# s = file("sysv_ipc_module.c").read()
# 
# if not re.search("""\s*//\s*#define\s+SYSV_IPC_DEBUG""", s):
#     print """
# ******************************************************
#   Are you sure that SYSV_IPC_DEBUG is commented out?
# ******************************************************
# """



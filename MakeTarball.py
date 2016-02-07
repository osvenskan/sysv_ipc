import tarfile
import os
import os.path

VERSION = file("VERSION").read().strip()

filenames = (
    "LICENSE",
    "INSTALL",
    "README",
    "VERSION",
    "ReadMe.html",
    "setup.py",
    "prober.py",
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
    "junk"
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

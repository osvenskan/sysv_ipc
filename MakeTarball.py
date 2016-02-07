#!/usr/bin/env python

# Python imports
import time
import tarfile
import os
import hashlib

RSS_TIMESTAMP_FORMAT = "%a, %d %b %Y %H:%M:%S GMT"

VERSION = open("VERSION").read().strip()

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
    "ftok_experiment.py",
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
    "demo/utils_for_2.py",
    "demo/utils_for_3.py",
    "demo/SampleIpcConversation.png",
    "demo2/",
    "demo2/ReadMe.txt",
    "demo2/cleanup.py",
    "demo2/conclusion.py",
    "demo2/params.txt",
    "demo2/premise.py",
    "demo2/utils.py",
    "demo2/utils_for_2.py",
    "demo2/utils_for_3.py",
    "demo2/SampleIpcConversation.png",
    "demo4/ReadMe.txt",
    "demo4/child.py",
    "demo4/parent.py",
    "prober.py",
    "prober/",
    "prober/semtimedop_test.c",
    "prober/probe_page_size.c",
    "prober/sniff_union_semun_defined.c",
)

tarball_name = "sysv_ipc-%s.tar.gz" % VERSION
md5_name = "sysv_ipc-%s.md5.txt" % VERSION
sha1_name = "sysv_ipc-%s.sha1.txt" % VERSION

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

# Generate the tarball hashes
s = open("./" + tarball_name).read()

md5 = hashlib.md5(s).hexdigest()

sha1 = hashlib.sha1(s).hexdigest()

print "md5 = {}, sha1 = {}".format(md5, sha1)

open(md5_name, "w").write(md5)
open(sha1_name, "w").write(sha1)



# Print an RSS item suitable for pasting into rss.xml
timestamp = time.strftime(RSS_TIMESTAMP_FORMAT, time.gmtime())

print """

		<item>
			<guid isPermaLink="false">%s</guid>
			<title>sysv_ipc %s Released</title>
			<pubDate>%s</pubDate>
			<link>http://semanchuk.com/philip/sysv_ipc/</link>
			<description>Version %s of sysv_ipc has been released.
			</description>
		</item>

""" % (VERSION, VERSION, timestamp, VERSION)


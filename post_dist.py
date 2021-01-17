#!/usr/bin/env python

# Python imports
import time
import hashlib
import shutil
import os

RSS_TIMESTAMP_FORMAT = "%a, %d %b %Y %H:%M:%S GMT"

with open("VERSION") as f:
    VERSION = f.read().strip()

# Make a copy of the tarball for posterity
tarball_name = "sysv_ipc-%s.tar.gz" % VERSION
shutil.copyfile(os.path.join("dist", tarball_name),
                os.path.join("releases", tarball_name))

tarball_name = "releases/sysv_ipc-%s.tar.gz" % VERSION
md5_name = "releases/sysv_ipc-%s.md5.txt" % VERSION
sha1_name = "releases/sysv_ipc-%s.sha1.txt" % VERSION

# Generate hashes of the tarball
tarball_content = open(tarball_name, 'rb').read()
for hash_function_name in ('md5', 'sha1', 'sha256'):
    hash_function = getattr(hashlib, hash_function_name)
    hash_value = hash_function(tarball_content).hexdigest()

    hash_filename = "releases/sysv_ipc-{}.{}.txt".format(VERSION, hash_function_name)

    open(hash_filename, "wb").write(hash_value.encode('ascii'))
    print(hash_function_name + " = " + hash_value)

# Print an RSS item suitable for pasting into rss.xml
timestamp = time.strftime(RSS_TIMESTAMP_FORMAT, time.gmtime())

print("""

        <item>
            <guid isPermaLink="false">%s</guid>
            <title>sysv_ipc %s Released</title>
            <pubDate>%s</pubDate>
            <link>http://semanchuk.com/philip/sysv_ipc/</link>
            <description>Version %s of sysv_ipc has been released.
            </description>
        </item>

""" % (VERSION, VERSION, timestamp, VERSION))

print("Don't forget this:\ngit tag rel" + VERSION)

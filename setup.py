# Python modules
import distutils.core as duc

# sysv_ipc installation helper module
import prober

VERSION = file("VERSION").read().strip()

name = "sysv_ipc"
description = "System V IPC primitives (semaphores and shared memory) for Python"
long_description = """Sysv_ipc gives access to System V shared memory and 
semaphores on *nix systems. These modules only work on platforms that 
support System V shared objects. Most *nixes do (including OS X) but 
Windows does not. Sample code is included.

Currently, this module only supports semaphores and shared memory. Support
for message queues is planned for a future version.

You might also be interested in the similar POSIX IPC module at:
http://semanchuk.com/philip/posix_ipc/
"""
author = "Philip Semanchuk",
author_email = "philip@semanchuk.com",
maintainer = "Philip Semanchuk",
url = "http://semanchuk.com/philip/sysv_ipc/",
download_url = "http://semanchuk.com/philip/sysv_ipc/sysv_ipc-%s.tar.gz" % VERSION,
source_files = ["sysv_ipc_module.c"]
# http://pypi.python.org/pypi?:action=list_classifiers
classifiers = [ "Development Status :: 3 - Alpha",
                "Intended Audience :: Developers", 
                "License :: OSI Approved :: GNU General Public License (GPL)",
                "Operating System :: MacOS :: MacOS X",
                "Operating System :: POSIX", 
                "Operating System :: Unix", 
                "Programming Language :: Python", 
                "Topic :: Utilities" ]
license = "http://gplv3.fsf.org/"
keywords = "ipc inter-process communication semaphore shared memory shm message queue"


prober.probe()

ext_modules = [duc.Extension("sysv_ipc", 
                             source_files,
#                            extra_compile_args=['-E']
                            )
              ]

duc.setup(name = name,
          version = VERSION,
          description = description,
          long_description = long_description,
          author = author,
          author_email = author_email,
          maintainer = maintainer,
          url = url,
          download_url = download_url,
          classifiers = classifiers,
          license = license,
          keywords = keywords,
          ext_modules = ext_modules
         )
              

# Python modules
import distutils.core as duc

# sysv_ipc installation helper module
import prober

VERSION = open("VERSION", "r").read().strip()

name = "sysv_ipc"
description = "System V IPC primitives (semaphores, shared memory and message queues) for Python"
f = open("README", "r")
long_description = f.read()
f.close()
author = "Philip Semanchuk",
author_email = "philip@semanchuk.com",
maintainer = "Philip Semanchuk",
url = "http://semanchuk.com/philip/sysv_ipc/",
download_url = "http://semanchuk.com/philip/sysv_ipc/sysv_ipc-%s.tar.gz" % VERSION,
source_files = ["sysv_ipc_module.c", "common.c", "semaphore.c", "memory.c", 
                "mq.c" ]
# http://pypi.python.org/pypi?:action=list_classifiers
classifiers = [ "Development Status :: 5 - Production/Stable", 
                "Intended Audience :: Developers", 
                "License :: OSI Approved :: BSD License",
                "Operating System :: MacOS :: MacOS X",
                "Operating System :: POSIX :: BSD :: FreeBSD",
                "Operating System :: POSIX :: Linux",
                "Operating System :: POSIX :: SunOS/Solaris",
                "Operating System :: POSIX", 
                "Operating System :: Unix", 
                "Programming Language :: Python", 
                "Programming Language :: Python :: 2",
                "Programming Language :: Python :: 3",
                "Topic :: Utilities" ]
license = "http://creativecommons.org/licenses/BSD/"
keywords = "ipc inter-process communication semaphore shared memory shm message queue"

prober.probe()

extension = duc.Extension("sysv_ipc", 
                          source_files,
#                         extra_compile_args=['-E']
                          depends = [ "common.c", "common.h", "memory.c", 
                                      "memory.h", "mq.c", "mq.h", 
                                      "probe_results.h", "semaphore.c", 
                                      "semaphore.h", "sysv_ipc_module.c", 
                                    ],
                         )

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
          ext_modules = [ extension ]
         )
              

# Python modules
import distutils.core
import sys

# sysv_ipc installation helper module
import prober

VERSION = file("VERSION").read().strip()

prober.probe()

#FIXME
description = """Sysv_ipc gives access to System V shared memory and semaphores on *nix systems.
These modules only work on 
platforms that support System V shared objects. Most *nixes do (including OS X) but 
Windows does not."""

# # patch distutils if it can't cope with the "classifiers" or
# # "download_url" keywords
# if sys.version < '2.2.3':
#     from distutils.dist import DistributionMetadata
#     DistributionMetadata.classifiers = None
#     DistributionMetadata.download_url = None

classifiers = [ 'Development Status :: 3 - Alpha', 
                'Intended Audience :: Developers', 
                'License :: Freely Distributable', 
                'License :: OSI Approved :: GNU General Public License (GPL)',
                'Operating System :: MacOS :: MacOS X',
                'Operating System :: POSIX', 
                'Operating System :: Unix', 
                'Programming Language :: Python', 
                'Topic :: Utilities' ]


macros_and_defines = [ ]

# AFAICT the semun union is supposed to be declared in one's code. However, a
# lot of legacy code gets this wrong and some header files define it, e.g. 
# sem.h on OS X where it's #ifdef-ed so that legacy code won't break. In fact 
# it's my #define of _XOPEN_SOURCE that causes it to not exist on darwin. 
# Notably, some (all?) Linux platforms #define _SEM_SEMUN_UNDEFINED if it's up
# to my code to declare this union, so I use that flag as my standard.
if "darwin" in sys.platform:
    macros_and_defines.append( ('_SEM_SEMUN_UNDEFINED', None) )


if prober.key_underscores == 2:
    macros_and_defines.append( ("TWO_UNDERSCORE_KEY", None) )
elif prober.key_underscores == 1:
    macros_and_defines.append( ("ONE_UNDERSCORE_KEY", None) )
else:
    macros_and_defines.append( ("ZERO_UNDERSCORE_KEY", None) )
    

if prober.semtimedop_available:
    macros_and_defines.append( ("SEMTIMEDOP_EXISTS", None) )


ext_modules = [distutils.core.Extension("sysv_ipc", ["sysv_ipc_module.c"],
                                        define_macros=macros_and_defines,
#                                        extra_compile_args=['-E']
                                       )
              ]
distutils.core.setup(name="sysv_ipc", 
                     version=VERSION,
                     author="Philip Semanchuk",
                     maintainer="Philip Semanchuk",
                     url="http://semanchuk.com/philip/sysv_ipc/",
                     description=description,
                     ext_modules=ext_modules, 
                     classifiers=classifiers)


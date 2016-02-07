# Python modules
import distutils.core
import sys

# sysv_ipc installation helper module
import prober

VERSION = file("VERSION").read().strip()

prober.probe()

description = """Sysv_ipc gives access to System V shared memory and 
semaphores on *nix systems. These modules only work on platforms that 
support System V shared objects. Most *nixes do (including OS X) but 
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


ext_modules = [distutils.core.Extension("sysv_ipc", ["sysv_ipc_module.c"],
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


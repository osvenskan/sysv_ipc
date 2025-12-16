import setuptools
from setuptools.extension import Extension
import sys

# When python -m build runs, sys.path contains a minimum of entries. I add the current directory
# to it (which is guaranteed by setuptools to be the project's root) so that I can import my
# build_support tools.
sys.path.append('.')

# sysv_ipc installation helper module
import prober

# As of April 2025, specifying the license metadata here (rather than in pyproject.toml) seems
# like the best solution for now. See https://github.com/osvenskan/posix_ipc/issues/68
LICENSE = "BSD-3-Clause"

# As of April 2025, use of tool.setuptools.ext-modules is stil experimental in pyproject.toml,
# so I specify the extension module here in setup.py.
SOURCE_FILES = [
    "sysv_ipc_module.c",
    "common.c",
    "semaphore.c",
    "memory.c",
    "mq.c"
]
DEPENDS = [
    "probe_results.h",
    "common.c",
    "common.h",
    "memory.c",
    "memory.h",
    "mq.c",
    "mq.h",
    "semaphore.c",
    "semaphore.h",
    "sysv_ipc_module.c",
]

prober.probe()

ext_modules = [Extension("sysv_ipc",
                         SOURCE_FILES,
                         depends=DEPENDS,
                         # -E is useful for debugging compile errors.
                         # extra_compile_args=['-E'],
                         )]

setuptools.setup(ext_modules=ext_modules, license=LICENSE)

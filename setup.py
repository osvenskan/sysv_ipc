import setuptools
from setuptools.extension import Extension
import sys

# When python -m build runs, sys.path contains a minimum of entries. I add the current directory
# to it (which is guaranteed by setuptools to be the project's root) so that I can import my
# build_support tools.
sys.path.append('.')
import build_support.discover_system_info

# As of April 2025, specifying the license metadata here (rather than in pyproject.toml) seems
# like the best solution for now. See https://github.com/osvenskan/posix_ipc/issues/68
LICENSE = "BSD-3-Clause"

# As of April 2025, use of tool.setuptools.ext-modules is stil experimental in pyproject.toml,
# so I specify the extension module here in setup.py.
SOURCE_FILES = [
    "src/sysv_ipc_module.c",
    "src/common.c",
    "src/semaphore.c",
    "src/memory.c",
    "src/mq.c"
]
DEPENDS = [
    "src/system_info.h",
    "src/common.c",
    "src/common.h",
    "src/memory.c",
    "src/memory.h",
    "src/mq.c",
    "src/mq.h",
    "src/semaphore.c",
    "src/semaphore.h",
    "src/sysv_ipc_module.c",
]

# Run discovery to create system_info.h (if needed).
build_support.discover_system_info.discover()

ext_modules = [Extension("sysv_ipc",
                         SOURCE_FILES,
                         depends=DEPENDS,
                         # -E is useful for debugging compile errors.
                         # extra_compile_args=['-E'],
                         )]

setuptools.setup(ext_modules=ext_modules, license=LICENSE)

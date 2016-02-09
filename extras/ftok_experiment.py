"""This is an experiment to see how often ftok() returns duplicate keys for different filenames."""
import sys
import os
import sysv_ipc

# Python 2's raw_input() has become input() in Python 3.
if sys.version_info[0] == 3:
    input_method = input
else:
    input_method = raw_input  # noqa - tell flake8 to chill

if len(sys.argv) == 2:
    start_path = sys.argv[1]
else:
    msg = "Start path? [Default = your home directory] "
    start_path = input_method(msg)
    if not start_path:
        start_path = "~"

# Expand paths that start with a tilde and then absolutize.
start_path = os.path.expanduser(start_path)
start_path = os.path.abspath(start_path)

# For every filename in the tree, generate a key and associate the filename
# with that key via a dictionary.
d = {}
nfilenames = 0
for path, dirnames, filenames in os.walk(start_path):
    for filename in filenames:
        # Fully qualify the path
        filename = os.path.join(path, filename)

        nfilenames += 1

        # print("Processing %s..." % filename)

        key = sysv_ipc.ftok(filename, 42, True)

        if key not in d:
            d[key] = []

        d[key].append(filename)

# Print statistics, including files with non-unique keys.
ndups = 0
for key in d:
    if len(d[key]) > 1:
        ndups += len(d[key])
        print(key, d[key])

print("Out of {0} unique filenames, I found {1} duplicate keys.".format(nfilenames, ndups))

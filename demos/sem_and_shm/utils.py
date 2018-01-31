import time
import sys

PY_MAJOR_VERSION = sys.version_info[0]

if PY_MAJOR_VERSION > 2:
    NULL_CHAR = 0
else:
    NULL_CHAR = '\0'


def say(s):
    who = sys.argv[0]
    if who.endswith(".py"):
        who = who[:-3]
    s = "%s@%1.6f: %s" % (who, time.time(), s)
    print(s)


def write_to_memory(memory, s):
    say("writing %s " % s)
    s += '\0'
    if PY_MAJOR_VERSION > 2:
        s = s.encode()
    memory.write(s)


def read_from_memory(memory):
    s = memory.read()
    if PY_MAJOR_VERSION > 2:
        s = s.decode()
    i = s.find('\0')
    if i != -1:
        s = s[:i]
    say("read %s" % s)

    return s


def read_params():
    params = {}

    with open("params.txt", "r") as f:
        for line in f:
            line = line.strip()
            if line:
                if line.startswith('#'):
                    # comment in input; ignore
                    pass
                else:
                    name, value = line.split('=')
                    name = name.upper().strip()

                    if name == "PERMISSIONS":
                        value = int(value, 8)
                    else:
                        value = int(value)

                    params[name] = value

    return params

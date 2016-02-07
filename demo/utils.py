import time 
import sys

def say(s):
    who = sys.argv[0]
    if who.endswith(".py"):
        who = who[:-3]
    print "%s@%1.6f: %s" % (who, time.time(), s)

def write_to_memory(memory, s):
    say("writing %s " % s)
    # Pad with NULLs in case I'm communicating with a C program.
    memory.write(s + (memory.size - len(s)) * '\0')

def read_from_memory(memory):
    s = memory.read().strip('\0')
    say("read %s" % s)

    return s

def read_params():
    params = { }
    
    f = open("params.txt", "r")
    
    for line in f:
        line = line.strip()
        if len(line):
            if line.startswith('#'):
                pass # comment in input, ignore
            else:
                name, value = line.split('=')
                name = name.upper().strip()
            
                if name == "PERMISSIONS":
                    value = int(value, 8)
                else:
                    value = int(value)
        
                #print "name = %s, value = %d" % (name, value)
            
                params[name] = value

    f.close()
    
    return params
    

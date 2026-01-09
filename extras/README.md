The files in `extras` are unofficial, unsupported, somewhat hacky tools I've created to explore (mis)behaviors related to SysV IPC. For instance, I wrote `explore_message_queue_receive.c` as part of https://github.com/osvenskan/sysv_ipc/issues/38 (and so I could file a bug report with non-Python code that demonstrated the problem). And `memory_leak_tests.py` is a tool that I run from time to time to try to make sure my `sysv_ipc` code doesn't contain any memory leak bugs.

Feel free to play around with these tools yourself. Have fun!

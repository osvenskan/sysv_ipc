This demonstrates that `sysv_ipc` implements Python's "buffer protocol" which
allows you to create `bytearray` and `memoryview` objects from
`sysv_ipc.SharedMemory` instances. The demo doesn't do anything exciting;
it's main value is in the code.

The `memoryview` type under Python 3 has some nice features that aren't
present under Python 2.

## Caveat

A `memoryview` is just that, a view on a chunk of memory. It has some
similarities to a raw C pointer, namely, speed and, in this case, danger).

The size of the `memoryview` is set when it's created, so shrinking the
underlying memory segment could be fatal to your code.

For instance, if process A creates a `memoryview` atop an 8k chunk
of `sysv_ipc.SharedMemory` and then process B shrinks that same
`sysv_ipc.SharedMemory` segment to 4k, process A will segfault when it tries
to access any part of the `memoryview` past byte 4096. `Sysv_ipc` can't
protect you from this because once the `memoryview` is created, `sysv_ipc`
isn't invoked for reads and writes to the `memoryview`.

In practice, I'm not sure if any platforms allow resizing SysV shared
memory segments.

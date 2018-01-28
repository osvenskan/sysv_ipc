typedef struct {
    PyObject_HEAD
    key_t key;
    int id;
    int read_only;
    void *address;
} SharedMemory;

/* Union for passing values to shm_set_ipc_perm_value() */
union ipc_perm_value {
    uid_t uid;
    gid_t gid;
    mode_t mode;
};

/* Object methods */
PyObject *SharedMemory_new(PyTypeObject *, PyObject *, PyObject *);
int SharedMemory_init(SharedMemory *, PyObject *, PyObject *);
void SharedMemory_dealloc(SharedMemory *);
PyObject *SharedMemory_attach(SharedMemory *, PyObject *, PyObject *);
PyObject *SharedMemory_detach(SharedMemory *);
PyObject *SharedMemory_read(SharedMemory *, PyObject *, PyObject *);
PyObject *SharedMemory_write(SharedMemory *, PyObject *, PyObject *);
PyObject *SharedMemory_remove(SharedMemory *);




int SharedMemory_getbuffer(PyObject *, Py_buffer *, int);

#if PY_MAJOR_VERSION > 2
int shm_get_buffer(SharedMemory *, Py_buffer *, int);
//int shm_get_buffer(PyObject *, Py_buffer *, int);
#else
Py_ssize_t shm_get_read_write_buffer(SharedMemory *, Py_ssize_t, const void **);
Py_ssize_t shm_get_segment_count(SharedMemory *, Py_ssize_t *);
int shm_get_buffer(SharedMemory *, Py_buffer *, int);
void shm_release_buffer(SharedMemory *, Py_buffer *);
#endif
//PyBufferProcs SharedMemory_as_buffer;



// PyBufferProcs SharedMemory_as_buffer = {
//     // this definition is only compatible with Python 3.3 and above
//     (getbufferproc)SharedMemory_getbuffer,
//     (releasebufferproc)0,  // we do not require any special release function
// };



/* Object attributes (read-write & read-only) */

PyObject *shm_get_uid(SharedMemory *);
int shm_set_uid(SharedMemory *, PyObject *);

PyObject *shm_get_gid(SharedMemory *);
int shm_set_gid(SharedMemory *, PyObject *);

PyObject *shm_get_mode(SharedMemory *);
int shm_set_mode(SharedMemory *, PyObject *);

PyObject *shm_get_key(SharedMemory *);
PyObject *shm_get_size(SharedMemory *);
PyObject *shm_get_address(SharedMemory *);
PyObject *shm_get_attached(SharedMemory *);
PyObject *shm_get_last_attach_time(SharedMemory *);
PyObject *shm_get_last_detach_time(SharedMemory *);
PyObject *shm_get_last_change_time(SharedMemory *);
PyObject *shm_get_creator_pid(SharedMemory *);
PyObject *shm_get_last_pid(SharedMemory *);
PyObject *shm_get_number_attached(SharedMemory *);
PyObject *shm_get_cuid(SharedMemory *);
PyObject *shm_get_cgid(SharedMemory *);

PyObject *shm_str(SharedMemory *);
PyObject *shm_repr(SharedMemory *);


/* Utility functions */
PyObject *shm_remove(int);

PyObject *shm_attach(SharedMemory *, void *, int);


#include "Python.h"
#include "structmember.h"

#include "common.h"
#include "memory.h"

PyObject *
shm_remove(int shared_memory_id) {
    struct shmid_ds shm_info;

    DPRINTF("removing shm with id %d\n", shared_memory_id);
    if (-1 == shmctl(shared_memory_id, IPC_RMID, &shm_info)) {
        switch (errno) {
            case EIDRM:
            case EINVAL:
                PyErr_Format(pExistentialException, 
                             "No shared memory with id %d exists", 
                             shared_memory_id);
            break;
        
            case EPERM:
                PyErr_SetString(pPermissionsException, 
                                "You do not have permission to remove the shared memory");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }        
        goto error_return;
    }
    
    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
shm_get_value(int shared_memory_id, enum GET_SET_IDENTIFIERS field) {
    struct shmid_ds shm_info;
    PyObject *pReturn = NULL;

    DPRINTF("Calling shmctl(...IPC_STAT...), field = %d\n", field);
    if (-1 == shmctl(shared_memory_id, IPC_STAT, &shm_info)) {
        switch (errno) {
            case EIDRM:
            case EINVAL:
                PyErr_Format(pExistentialException, 
                             "No shared memory with id %d exists", 
                             shared_memory_id);
            break;
        
            case EACCES:
                PyErr_SetString(pPermissionsException, 
                                "You do not have permission to read the shared memory attribute");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }        

        goto error_return;
    }

    switch (field) {
        case SHM_SIZE:
            pReturn = SIZE_T_TO_PY(shm_info.shm_segsz);
        break;

        case SHM_LAST_ATTACH_TIME:
            pReturn = TIME_T_TO_PY(shm_info.shm_atime);
        break;
        
        case SHM_LAST_DETACH_TIME:
            pReturn = TIME_T_TO_PY(shm_info.shm_dtime);
        break;
        
        case SHM_LAST_CHANGE_TIME:
            pReturn = TIME_T_TO_PY(shm_info.shm_ctime);
        break;

        case SHM_CREATOR_PID:
            pReturn = PID_T_TO_PY(shm_info.shm_cpid);
        break;

        case SHM_LAST_AT_DT_PID:
            pReturn = PID_T_TO_PY(shm_info.shm_lpid);
        break;

        case SHM_NUMBER_ATTACHED:
            // shm_nattch is unsigned
            // ref: http://www.opengroup.org/onlinepubs/007908799/xsh/sysshm.h.html
            pReturn = py_int_or_long_from_ulong(shm_info.shm_nattch);
        break;

        case IPC_PERM_UID:            
            pReturn = UID_T_TO_PY(shm_info.shm_perm.uid);
        break;

        case IPC_PERM_GID:
            pReturn = GID_T_TO_PY(shm_info.shm_perm.gid);
        break;

        case IPC_PERM_CUID:
            pReturn = UID_T_TO_PY(shm_info.shm_perm.cuid);
        break;

        case IPC_PERM_CGID:
            pReturn = GID_T_TO_PY(shm_info.shm_perm.cgid);
        break;

        case IPC_PERM_MODE:
            pReturn = MODE_T_TO_PY(shm_info.shm_perm.mode);
        break;

        default:
            PyErr_Format(pInternalException, "Bad field %d passed to shm_get_value", field);
            goto error_return;
        break;
    }
    
    return pReturn;

    error_return:
    return NULL;
}


static int
shm_set_ipc_perm_value(int id, enum GET_SET_IDENTIFIERS field, union ipc_perm_value value) {
    struct shmid_ds shm_info;
    
    if (-1 == shmctl(id, IPC_STAT, &shm_info)) {
        switch (errno) {
            case EIDRM:
            case EINVAL:
                PyErr_Format(pExistentialException, 
                             "No shared memory with id %d exists", id);
            break;
        
            case EACCES:
                PyErr_SetString(pPermissionsException, 
                                "You do not have permission to read the shared memory attribute");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }
        goto error_return;
    }
    
    switch (field) {
        case IPC_PERM_UID:
            shm_info.shm_perm.uid = value.uid;
        break;
        
        case IPC_PERM_GID:
            shm_info.shm_perm.gid = value.gid;
        break;
        
        case IPC_PERM_MODE:
            shm_info.shm_perm.mode = value.mode;
        break;

        default:
            PyErr_Format(pInternalException, 
                         "Bad field %d passed to shm_set_ipc_perm_value", 
                         field);
            goto error_return;
        break;
    }
    
    if (-1 == shmctl(id, IPC_SET, &shm_info)) {
        switch (errno) {
            case EIDRM:
            case EINVAL:
                PyErr_Format(pExistentialException, 
                             "No shared memory with id %d exists", id);
            break;
        
            case EPERM:
                PyErr_SetString(pPermissionsException, 
                                "You do not have permission to change the shared memory's attributes");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }        

        goto error_return;
    }
    
    return 0;

    error_return:
    return -1;
}


void 
SharedMemory_dealloc(SharedMemory *self) {
    self->ob_type->tp_free((PyObject*)self); 
}

PyObject *
SharedMemory_new(PyTypeObject *type, PyObject *args, PyObject *kwlist) {
    SharedMemory *self;
    
    self = (SharedMemory *)type->tp_alloc(type, 0);

    return (PyObject *)self; 
}


int 
SharedMemory_init(SharedMemory *self, PyObject *args, PyObject *keywords) {
    int mode = 0600;
    unsigned long size = 0;
    int shmget_flags = 0;
    int shmat_flags = 0;
    char init_character = ' ';
    char *keyword_list[ ] = {"key", "flags", "mode", "size", "init_character", NULL};
    PyObject *py_size = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "O&|iikc", keyword_list, 
                                     &convert_key_param, &(self->key), 
                                     &shmget_flags, &mode, &size, 
                                     &init_character))
        goto error_return;
    
    mode &= 0777;
    shmget_flags &= ~0777;
    
    // When creating a new segment, the default size is PAGE_SIZE.
    if (((shmget_flags & IPC_CREX) == IPC_CREX) && (!size))
        size = PAGE_SIZE;

    DPRINTF("Calling shmget, key=%ld, size=%lu, mode=%o, flags=0x%x\n", 
            (long)self->key, size, mode, shmget_flags);
    self->id = shmget(self->key, size, mode | shmget_flags);
        
    DPRINTF("id == %d\n", self->id);
    
    if (self->id == -1) {
        switch (errno) {
            case EACCES:
                PyErr_Format(pPermissionsException, 
                             "Permission %o cannot be granted on the existing segment", 
                             mode);
            break;

            case EEXIST:
                PyErr_Format(pExistentialException, 
                    "Shared memory with the key %ld already exists", 
                    (long)self->key);
            break;

            case ENOENT:
                PyErr_Format(pExistentialException, 
                    "No shared memory exists with the key %ld", (long)self->key);
            break;

            case EINVAL:
                PyErr_SetString(PyExc_ValueError, "The size is invalid");
            break;

            case ENOMEM:
                PyErr_SetString(PyExc_MemoryError, "Not enough memory");
            break;

            case ENOSPC:
                PyErr_SetString(PyExc_OSError, 
                    "Not enough shared memory identifiers available (ENOSPC)");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }
        goto error_return;
    }

    // If no write permissions requested, attach read-only
    shmat_flags = (mode & 0200) ? 0 : SHM_RDONLY;
    DPRINTF("attaching memory with id %d using flags 0x%x\n", self->id, shmat_flags);
    self->address = shmat(self->id, NULL, shmat_flags);
    
    if ( (void *)-1 == self->address) {
        self->address = NULL;
        switch (errno) {
            case EACCES:
                PyErr_SetString(pPermissionsException, "No permission to attach");
            break;

            case ENOMEM:
                PyErr_SetString(PyExc_MemoryError, "Not enough memory");
            break;

            case EINVAL:
                // EINVAL from shmat() means id or address was invalid. 
                // Since my code (and not the caller) supplied both here,
                // I can't see a realistic way for it to occur so I'm not
                // going to bother handling it with a custom message.
            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }

        goto error_return;
    }
    
    if ( ((shmget_flags & IPC_CREX) == IPC_CREX) && (!(shmat_flags & SHM_RDONLY)) ) {
        // Initialize the memory.
        
        py_size = shm_get_value(self->id, SHM_SIZE);
    
        if (!py_size)
            goto error_return;
        else {
            size = PyInt_AsUnsignedLongMask(py_size);

            DPRINTF("memsetting address %p to %lu bytes of ASCII 0x%x (%c)\n", \
                    self->address, size, (int)init_character, init_character);
            memset(self->address, init_character, size);
        }

        Py_DECREF(py_size);
    }
    
    return 0;
    
    error_return:
    return -1;
}


PyObject *
SharedMemory_attach(SharedMemory *self, PyObject *args) {
    PyObject *py_address = NULL;
    void *address = NULL;
    int flags = 0;
    
    if (!PyArg_ParseTuple(args, "|Oi", &py_address, &flags))
        goto error_return;

    if ((!py_address) || (py_address == Py_None))
        address = NULL;
    else {
        if (PyLong_Check(py_address))
            address = PyLong_AsVoidPtr(py_address);
        else {
            PyErr_SetString(PyExc_TypeError, "address must be a long");
            goto error_return;
        }
    }
    
    self->address = shmat(self->id, address, flags);
    
    if ((void *)-1 == self->address) {
        self->address = NULL;
        switch (errno) {
            case EACCES:
                PyErr_SetString(pPermissionsException, "No permission to attach");
            break;

            case EINVAL:
                PyErr_SetString(PyExc_ValueError, "Invalid address or flags");
            break;

            case ENOMEM:
                PyErr_SetString(PyExc_MemoryError, "Not enough memory");
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }
        goto error_return;
    }

    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


PyObject *
SharedMemory_detach(SharedMemory *self) {
    if (-1 == shmdt(self->address)) {
        self->address = NULL;
        switch (errno) {
            case EINVAL:
                PyErr_SetNone(pNotAttachedException);
            break;

            default:
                PyErr_SetFromErrno(PyExc_OSError);
            break;
        }
        goto error_return;
    }

    self->address = NULL;

    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


PyObject *
SharedMemory_read(SharedMemory *self, PyObject *args, PyObject *keywords) {
    /* Tricky business here. A memory segment's size is a size_t which is 
       ulong or smaller. However, the largest string that Python can 
       construct is of ssize_t which is long or smaller. Therefore, the
       size and offset variables must be ulongs while the byte_count 
       must be a long (and must not exceed LONG_MAX).
       Mind your math!
    */
    long byte_count = 0;
    unsigned long offset = 0;
    unsigned long size;
    PyObject *py_size;
    char *keyword_list[ ] = {"byte_count", "offset", NULL};
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|lk", keyword_list, 
                                     &byte_count, &offset))
        goto error_return;
        
    if (self->address == NULL) {
        PyErr_SetString(pNotAttachedException, 
                        "Read attempt on unattached memory segment");
        goto error_return;
    }
    
    if ( (py_size = shm_get_value(self->id, SHM_SIZE)) ) {
        size = PyInt_AsUnsignedLongMask(py_size);
        Py_DECREF(py_size);
    }
    else
        goto error_return;

    DPRINTF("offset = %lu, byte_count = %ld, size = %lu\n",
            offset, byte_count, size);
    
    if (offset >= size) {
        PyErr_SetString(PyExc_ValueError, "The offset must be less than the segment size");
        goto error_return;
    }
    
    if (byte_count < 0) {
        PyErr_SetString(PyExc_ValueError, "The byte_count cannot be negative");
        goto error_return;
    }
    
    /* If the caller didn't specify a byte count or specified one that would
       read past the end of the segment, return everything from the offset to
       the end of the segment.
       Be careful here not to express the second if condition w/addition, e.g.
            (byte_count + offset > size)
       It might be more intuitive but since byte_count is a long and offset 
       is a ulong, their sum could cause an arithmetic overflow. */
    if ((!byte_count) || ((unsigned long)byte_count > size - offset)) {
        // byte_count needs to be calculated
        if (size - offset <= (unsigned long)PY_STRING_LENGTH_MAX)
            byte_count = size - offset;
        else {
            // Caller is asking for more bytes than I can stuff into 
            // a Python string.
            PyErr_Format(PyExc_ValueError,
                         "The byte_count cannot exceed Python's max string length %ld", 
                         (long)PY_STRING_LENGTH_MAX);
            goto error_return;
        }
    }
        
    return PyString_FromStringAndSize(self->address + offset, byte_count);
    
    error_return:
    return NULL;
}


PyObject *
SharedMemory_write(SharedMemory *self, PyObject *args) {
    /* See comments for read() regarding "size issues". Note that here
       Python provides the byte_count so it can't be negative. 
       
       In Python >= 2.5, the Python argument specifier 's#' expects a 
       py_ssize_t for its second parameter. A long is long enough. */
    const char *buffer;
    long byte_count;
    unsigned long offset = 0;
    unsigned long size;
    PyObject *py_size;

    if (!PyArg_ParseTuple(args, "s#|k", &buffer, &byte_count, &offset))
        goto error_return;
	    
    if (self->address == NULL) {
        PyErr_SetString(pNotAttachedException, "Write attempt on unattached memory segment");
        goto error_return;
    }
    
    if ( (py_size = shm_get_value(self->id, SHM_SIZE)) ) {
        size = PyInt_AsUnsignedLongMask(py_size);
        Py_DECREF(py_size);
    }
    else
        goto error_return;

    if ((unsigned long)byte_count > size - offset) {
        PyErr_SetString(PyExc_ValueError, "Attempt to write past end of memory segment");
        goto error_return;
    }

    memcpy((self->address + offset), buffer, byte_count);

    Py_RETURN_NONE;

    error_return:
    return NULL;
}


PyObject *
SharedMemory_remove(SharedMemory *self) {
    return shm_remove(self->id);
}


PyObject *
shm_get_key(SharedMemory *self) {
    return KEY_T_TO_PY(self->key);
}

PyObject *
shm_get_size(SharedMemory *self) {
    return shm_get_value(self->id, SHM_SIZE);
}

PyObject *
shm_get_address(SharedMemory *self) {
    return PyLong_FromVoidPtr(self->address);
}

PyObject *
shm_get_attached(SharedMemory *self) {
    if (self->address) 
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

PyObject *
shm_get_last_attach_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_ATTACH_TIME);
}

PyObject *
shm_get_last_detach_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_DETACH_TIME);
}

PyObject *
shm_get_last_change_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_CHANGE_TIME);
}

PyObject *
shm_get_creator_pid(SharedMemory *self) {
    return shm_get_value(self->id, SHM_CREATOR_PID);
}

PyObject *
shm_get_last_pid(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_AT_DT_PID);
}

PyObject *
shm_get_number_attached(SharedMemory *self) {
    return shm_get_value(self->id, SHM_NUMBER_ATTACHED);
}

PyObject *
shm_get_uid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_UID);
}

PyObject *
shm_get_cuid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CUID);
}

PyObject *
shm_get_cgid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CGID);
}

PyObject *
shm_get_mode(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_MODE);
}

int
shm_set_uid(SharedMemory *self, PyObject *py_value) {
    union ipc_perm_value new_value;
    
    if (!PyInt_Check(py_value)) {
		PyErr_SetString(PyExc_TypeError, "Attribute 'uid' must be an integer");
        goto error_return;
    }
    
    new_value.uid = PyInt_AsLong(py_value);
    
    if (((uid_t)-1 == new_value.uid) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_UID, new_value);

    error_return:
    return -1;
}


PyObject *
shm_get_gid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_GID);
}

int
shm_set_gid(SharedMemory *self, PyObject *py_value) {
    union ipc_perm_value new_value;
    
    if (!PyInt_Check(py_value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'gid' must be an integer");
        goto error_return;
    }
    
    new_value.gid = PyInt_AsLong(py_value);
    
    if (((gid_t)-1 == new_value.gid) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_GID, new_value);

    error_return:
    return -1;
}

int
shm_set_mode(SharedMemory *self, PyObject *py_value) {
    union ipc_perm_value new_value;
    
    if (!PyInt_Check(py_value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'mode' must be an integer");
        goto error_return;
    }
    
    new_value.mode = PyInt_AsLong(py_value);
    
    if (((mode_t)-1 == new_value.mode) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_MODE, new_value);

    error_return:
    return -1;
}


/*
    sysv_ipc - A Python module for accessing System V semaphores
                and shared memory.

    Copyright (C)  2008 Philip Semanchuk

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#if PY_MAJOR_VERSION >= 0x02050000
#define PY_SSIZE_T_CLEAN
#define PY_STRING_LENGTH_MAX  PY_SSIZE_T
#else
#define PY_STRING_LENGTH_MAX  INT_MAX
#endif


#include "Python.h"
#include "structmember.h"

// for memset
#include <string.h>

// For the math surrounding timeouts for semtimedop()
#include <math.h>

#include "probe_results.h"

#include <sys/ipc.h>		/* for system's IPC_xxx definitions */
#include <sys/shm.h>		/* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>		/* for semget, semctl, semop */

enum GET_SET_IDENTIFIERS {
    IPC_PERM_UID = 1,
    IPC_PERM_GID,
    IPC_PERM_CUID,
    IPC_PERM_CGID,
    IPC_PERM_MODE,
    SEM_OTIME,
    SHM_SIZE,
    SHM_LAST_ATTACH_TIME,
    SHM_LAST_DETACH_TIME,
    SHM_LAST_CHANGE_TIME,
    SHM_CREATOR_PID,
    SHM_LAST_AT_DT_PID,
    SHM_NUMBER_ATTACHED
};

// This enum has to start at zero because it's values are used as an 
// arry index in sem_perform_semop().
enum SEMOP_TYPE {
    SEMOP_P = 0,
    SEMOP_V,
    SEMOP_Z
};

// Shorthand for lazy typists like me
#define IPC_CREX  (IPC_CREAT | IPC_EXCL)

/*
      Exceptions for this module      
*/

static PyObject *pBaseException;
static PyObject *pInternalException;
static PyObject *pPermissionsException;
static PyObject *pExistentialException;
static PyObject *pBusyException;
static PyObject *pNotAttachedException;


typedef struct {
    PyObject_HEAD
    key_t key;
    int id;
    void *address;
} SharedMemory;

typedef struct {
    PyObject_HEAD
    key_t key;
    int id;
    short op_flags;
} Semaphore;


// SUSv3 guarantees a uid_t to be an integer type. Some code returns 
// (uid_t)-1, so I guess this has to be a signed type.
// ref: http://www.opengroup.org/onlinepubs/009695399/basedefs/sys/types.h.html
// ref: http://www.opengroup.org/onlinepubs/9699919799/functions/chown.html
#define UID_T_TO_PY(uid)   PyInt_FromLong(uid)

// SUSv3 guarantees a gid_t to be an integer type. Some code returns 
// (gid_t)-1, so I guess this has to be a signed type.
// ref: http://www.opengroup.org/onlinepubs/009695399/basedefs/sys/types.h.html
#define GID_T_TO_PY(gid)   PyInt_FromLong(gid)

// I'm not sure what guarantees SUSv3 makes about a mode_t, but param 3 of
// shmget() is an int that contains flags in addition to the mode, so
// mode must be able to fit into an int.
// ref: http://www.opengroup.org/onlinepubs/009695399/functions/shmget.html
#define MODE_T_TO_PY(mode)   PyInt_FromLong(mode)

// I'm not sure what guarantees SUSv3 makes about a time_t, but the times
// I deal with here are all guaranteed to be after 1 Jan 1970 which means
// they'll always be positive numbers. A ulong sounds appropriate to me,
// and Python agrees in posixmodule.c.
#define TIME_T_TO_PY(time)   py_int_or_long_from_ulong(time)

// C89 guarantees a size_t to be unsigned and fit into a ulong or smaller.
#define SIZE_T_TO_PY(size)   py_int_or_long_from_ulong(size)

// SUSv3 guarantees a pid_t to be a signed integer type. Some code returns 
// (pid_t)-1 so I guess this has to be signed.
// ref: http://www.opengroup.org/onlinepubs/000095399/basedefs/sys/types.h.html#tag_13_67
#define PID_T_TO_PY(pid)   PyInt_FromLong(pid)

// It is recommended practice to define this union here in the .c module, but
// it's been common practice for platforms to define it themselves in header 
// files. For instance, BSD and OS X do so (provisionally) in sem.h. As a 
// result, I need to surround this with an #ifdef.
#ifdef _SEM_SEMUN_UNDEFINED
union semun {
    int val;                    /* used for SETVAL only */
    struct semid_ds *buf;       /* for IPC_STAT and IPC_SET */
    unsigned short *array;      /* used for GETALL and SETALL */
};
#endif

/* Union for passing values to shm_set_ipc_perm_value() */
union ipc_perm_value {
    uid_t uid;
    gid_t gid;
    mode_t mode;
};

/* Struct to contain a timeout which can be None */
typedef struct {
    int is_none;
    int is_zero;
    struct timespec timestamp;
} NoneableTimeout;

/* Struct to contain a semop delta which can be None */
typedef struct {
    int is_none;
    long value;
} NoneableLong;


#define ONE_BILLION 1000000000

#ifdef SYSV_IPC_DEBUG
#define DPRINTF(fmt, args...) fprintf(stderr, "+++ " fmt, ## args)
#else
#define DPRINTF(fmt, args...)
#endif


/* Utility functions */
static int 
convert_timeout(PyObject *py_timeout, void *converted_timeout) {
    // Converts a PyObject into a timeout if possible. The PyObject should
    // be None or some sort of numeric value (e.g. int, float, etc.)
    // converted_timeout should point to a NoneableTimeout. When this function
    // returns, if the NoneableTimeout's is_none is true, then the rest of the
    // struct is undefined. Otherwise, the rest of the struct is populated.
    int rc = 0;
    double simple_timeout = 0;
    NoneableTimeout *p_timeout = (NoneableTimeout *)converted_timeout;

    // The timeout can be None or any Python numeric type (float, 
    // int, long).
    if (py_timeout == Py_None)
        rc = 1;
    else if (PyFloat_Check(py_timeout)) {
        rc = 1;
        simple_timeout = PyFloat_AsDouble(py_timeout);
    }
    else if (PyInt_Check(py_timeout)) {
        rc = 1;
        simple_timeout = (double)PyInt_AsLong(py_timeout);
    }
    else if (PyLong_Check(py_timeout)) {
        rc = 1;
        simple_timeout = (double)PyLong_AsLong(py_timeout);
    }
    
    // The timeout may not be negative.
    if ((rc) && (simple_timeout < 0))
        rc = 0;

    if (!rc)
        PyErr_SetString(PyExc_TypeError, 
                        "The timeout must be None or a non-negative number");
    else {
        if (py_timeout == Py_None)
            p_timeout->is_none = 1;
        else {
            p_timeout->is_none = 0;
            
            p_timeout->is_zero = (!simple_timeout);

            // Note the difference between this and POSIX timeouts. System V 
            // timeouts expect tv_sec to represent a delta from the current 
            // time whereas POSIX semaphores expect an absolute value.
            p_timeout->timestamp.tv_sec = (time_t)floor(simple_timeout);
            p_timeout->timestamp.tv_nsec = (long)((simple_timeout - floor(simple_timeout)) * ONE_BILLION);
        }
    }
        
    return rc;
}

static PyObject *
py_int_or_long_from_ulong(unsigned long value) {
    // Python ints are guaranteed to accept up to LONG_MAX. Anything 
    // larger needs to be a Python long.
    if (value > LONG_MAX)
        return PyLong_FromUnsignedLong(value);
    else
        return PyInt_FromLong(value);
}


void 
sem_set_error(void) {
    switch (errno) {
        case ENOENT:
        case EINVAL:
            PyErr_Format(pExistentialException, 
                         "No semaphore exists with the specified key");
        break;

        case EEXIST:
            PyErr_Format(pExistentialException, "A semaphore with the specified key already exists");
        break;

        case EACCES:
            PyErr_SetString(pPermissionsException, "No permission to access this semaphore");
        break;
        
        case ERANGE:
            PyErr_Format(PyExc_ValueError, 
                "The semaphore's value must remain between 0 and %ld (SEMAPHORE_VALUE_MAX)", 
                (long)SEMAPHORE_VALUE_MAX);
        break;
        
        case EAGAIN:
            PyErr_SetString(pBusyException, "Semaphore is busy");
        break;

        case EIDRM:
            PyErr_SetString(pExistentialException, "The semaphore was removed");
        break;
        
        case EINTR:
            PyErr_SetString(pBaseException, "Signaled while waiting");
        break;
        
        case ENOMEM:
            PyErr_SetString(PyExc_MemoryError, "Not enough memory");
        break;

        default:
            PyErr_SetFromErrno(PyExc_OSError);
        break;
    }    
}

/*

      Semaphore implementation functions
        
*/


static PyObject *
sem_perform_semop(enum SEMOP_TYPE op_type, Semaphore *self, PyObject *args, PyObject *keywords) {
    int rc = 0;
    NoneableTimeout timeout;
    struct sembuf op[1];
    /* delta (a.k.a. struct sembuf.sem_op) is a short
       ref: http://www.opengroup.org/onlinepubs/000095399/functions/semop.html
    */
    short int delta;
    static char *keyword_list[3][3] = {
                    {"timeout", "delta", NULL},     // P
                    {"delta", NULL},                // V
                    {"timeout", NULL}               // Z
                };
                
                
    /* Initialize this to the default value. If the user doesn't pass a
       timeout, Python won't call convert_timeout() and so the timeout
       will be otherwise uninitialized.
    */
    timeout.is_none = 1;

    /* op_type is P, V or Z corresponding to the 3 Semaphore methods 
       that call that call semop(). */
    switch (op_type) {
        case SEMOP_P:
            delta = -1;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|O&h",
                                             keyword_list[SEMOP_P], 
                                             convert_timeout, &timeout,
                                             &delta);

            if (rc && !delta) {
                rc = 0;
                PyErr_SetString(PyExc_ValueError, "The delta must be non-zero");
            }
            else
                delta = -abs(delta);
        break;
            
        case SEMOP_V:
            delta = 1;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|h", 
                                             keyword_list[SEMOP_V], 
                                             &delta);

            if (rc && !delta) {
                rc = 0;
                PyErr_SetString(PyExc_ValueError, "The delta must be non-zero");
            }
            else
                delta = abs(delta);
        break;
        
        case SEMOP_Z:
            delta = 0;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|O&",
                                             keyword_list[SEMOP_Z],
                                             convert_timeout, &timeout);
        break;
        
        default:
            PyErr_Format(pInternalException, "Bad op_type (%d)", op_type);
            rc = 0;
        break;
    }

    if (!rc) 
        goto error_return;

    // Now that the caller's params have been vetted, I set up the op struct
    // that I'm going to pass to semop().
    op[0].sem_num = 0;
    op[0].sem_op = delta;
    op[0].sem_flg = self->op_flags;

    Py_BEGIN_ALLOW_THREADS    
#ifdef SEMTIMEDOP_EXISTS
    // Call semtimedop() if appropriate, otherwise call semop()
    if (!timeout.is_none) {
        DPRINTF("calling semtimedop on id %d, op.sem_op = %d, op.flags=%x\n",
                self->id, op[0].sem_op, op[0].sem_flg);
        DPRINTF("timeout tv_sec = %ld; timeout tv_nsec = %ld\n", 
                timeout.timestamp.tv_sec, timeout.timestamp.tv_nsec);
        rc = semtimedop(self->id, op, 1, &timeout.timestamp);
    }
    else {
        DPRINTF("calling semop on id %d, op.sem_op = %d, op.flags=%x\n",
                self->id, op[0].sem_op, op[0].sem_flg);
        rc = semop(self->id, op, 1);
    }
#else 
    // no support for semtimedop(), always call semop() instead.
    DPRINTF("calling semop on id %d, op.sem_op = %d, op.flags=%x\n",
            self->id, op[0].sem_op, op[0].sem_flg);
    rc = semop(self->id, op, 1);
#endif
    Py_END_ALLOW_THREADS

    if (rc == -1) {
        sem_set_error();
        goto error_return;
    }

    Py_RETURN_NONE;
    
    error_return:
    return NULL;
}


// cmd can be any of the values defined in the documentation for semctl(). 
static PyObject *
sem_get_semctl_value(int semaphore_id, int cmd) {
    int rc;
    
    rc = semctl(semaphore_id, 0, cmd);
    
    if (-1 == rc) {
        sem_set_error();
        goto error_return;
    }

    return PyInt_FromLong(rc);

    error_return:
    return NULL;    
}


static PyObject *
sem_get_ipc_perm_value(int id, enum GET_SET_IDENTIFIERS field) {
    struct semid_ds sem_info;
    union semun arg;
    PyObject *pValue = NULL;
    
    arg.buf = &sem_info;
    
    // Here I get the values currently associated with the semaphore. 
    if (-1 == semctl(id, 0, IPC_STAT, arg)) {
        sem_set_error();
        goto error_return;
    }
    
    switch (field) {
        case IPC_PERM_UID:
            pValue = UID_T_TO_PY(sem_info.sem_perm.uid);
        break;

        case IPC_PERM_GID:
            pValue = GID_T_TO_PY(sem_info.sem_perm.gid);
        break;
        
        case IPC_PERM_CUID:
            pValue = UID_T_TO_PY(sem_info.sem_perm.cuid);
        break;
        
        case IPC_PERM_CGID:
            pValue = GID_T_TO_PY(sem_info.sem_perm.cgid);
        break;
        
        case IPC_PERM_MODE:
            pValue = MODE_T_TO_PY(sem_info.sem_perm.mode);
        break;
        
        // This isn't an ipc_perm value but it fits here anyway.
        case SEM_OTIME:
            pValue = TIME_T_TO_PY(sem_info.sem_otime);
        break;
        
        default:
            PyErr_Format(pInternalException, "Bad field %d passed to sem_get_ipc_perm_value", field);
            goto error_return;
        break;
    }
    
    return pValue;

    error_return:
    return NULL;    
}


static int
sem_set_ipc_perm_value(int id, enum GET_SET_IDENTIFIERS field, PyObject *py_value) {
    struct semid_ds sem_info;
    union semun arg;
    
    arg.buf = &sem_info;

    if (!PyInt_Check(py_value)) {
        PyErr_Format(PyExc_TypeError, "The attribute must be an integer");
        goto error_return;
    }
    
    arg.buf = &sem_info;

    /* Here I get the current values associated with the semaphore. It's 
       critical to populate sem_info with current values here (rather than 
       just using the struct filled with whatever garbage it acquired from
       being declared on the stack) because the call to semctl(...IPC_SET...)
       below will copy uid, gid and mode to the kernel's data structure. 
    */
    if (-1 == semctl(id, 0, IPC_STAT, arg)) {
        sem_set_error();
        goto error_return;
    }
    
    switch (field) {
        case IPC_PERM_UID:
            sem_info.sem_perm.uid = PyInt_AsLong(py_value);
        break;
        
        case IPC_PERM_GID:
            sem_info.sem_perm.gid = PyInt_AsLong(py_value);
        break;
        
        case IPC_PERM_MODE:
            sem_info.sem_perm.mode = PyInt_AsLong(py_value);
        break;

        default:
            PyErr_Format(pInternalException, "Bad field %d passed to sem_set_ipc_perm_value", field);
            goto error_return;
        break;
    }
    
    if (-1 == semctl(id, 0, IPC_SET, arg)) {
        sem_set_error();
        goto error_return;
    }
    
    return 0;

    error_return:
    return -1;
}

static void 
Semaphore_dealloc(Semaphore *self) {
    self->ob_type->tp_free((PyObject*)self); 
}

static PyObject *
Semaphore_new(PyTypeObject *type, PyObject *args, PyObject *keywords) {
    Semaphore *self;
    
    self = (Semaphore *)type->tp_alloc(type, 0);

    return (PyObject *)self; 
}


static int 
Semaphore_init(Semaphore *self, PyObject *args, PyObject *keywords) {
    long key;
    int mode = 0600;
    int initial_value = 0;
    int flags = 0;
    union semun arg;
    static char *keyword_list[ ] = {"key", "flags", "mode", "initial_value", NULL};
    
    //Semaphore(key, [flags = 0, [mode = 0600, [initial_value = 0]]])
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "l|iii", keyword_list, 
                                        &key, &flags, &mode, &initial_value))
        goto error_return;
    
    if ((key < 0) || (key > KEY_MAX)) {
        PyErr_Format(PyExc_ValueError, "Key must be between 0 and %ld (KEY_MAX)", (long)KEY_MAX);
        goto error_return;
    }
        
    self->key = key;
    self->op_flags = 0;
        
    // I mask the caller's flags against the two IPC_* flags to ensure that 
    // nothing funky sneaks into the flags.
    flags &= (IPC_CREAT | IPC_EXCL);
    
    // Note that Sys V sems can be in "sets" (arrays) but I hardcode this to
    // always be a set with just one member.
    // Permissions and flags (i.e. IPC_CREAT | IPC_EXCL) are both crammed into
    // the 3rd param.
    DPRINTF("Calling semget, key=%ld, mode=%o, flags=%x\n", (long)self->key, mode, flags);
    self->id = semget(self->key, 1, mode | flags);
        
    DPRINTF("id == %d\n", self->id);
    
    if (self->id == -1) {
        sem_set_error();
        goto error_return;
    }

    // Before attempting to set the initial value, I have to be sure that
    // I created this semaphore and that I have write access to it.
    if ((flags & (IPC_CREAT | IPC_EXCL)) && (mode & 0200)) {
        DPRINTF("setting initial value to %d\n", initial_value);
        arg.val = initial_value;

        if (-1 == semctl(self->id, 0, SETVAL, arg)) {
            sem_set_error();
            goto error_return;
        }
    }
    
    return 0; 
    
    error_return:
    return -1;
}


static PyObject *
Semaphore_P(Semaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_P, self, args, keywords);
}


static PyObject *
Semaphore_acquire(Semaphore *self, PyObject *args, PyObject *keywords) {
    return Semaphore_P(self, args, keywords);
}


static PyObject *
Semaphore_V(Semaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_V, self, args, keywords);
}


static PyObject *
Semaphore_release(Semaphore *self, PyObject *args, PyObject *keywords) {
    return Semaphore_V(self, args, keywords);
}


static PyObject *
Semaphore_Z(Semaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_Z, self, args, keywords);
}


static PyObject *
Semaphore_remove(Semaphore *self) {
    if (NULL == sem_get_semctl_value(self->id, IPC_RMID))
        goto error_return;
    
    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
sem_get_value(Semaphore *self) {
    return sem_get_semctl_value(self->id, GETVAL);
}


static int
sem_set_value(Semaphore *self, PyObject *py_value)
{
    union semun arg;
    long value;

    if (!PyInt_Check(py_value)) {
		PyErr_Format(PyExc_TypeError, "Attribute 'value' must be an integer");
        goto error_return;
    }
    
    value = PyInt_AsLong(py_value);
    
    DPRINTF("C value is %ld\n", value);
    
    if ((-1 == value) && PyErr_Occurred()) {
        // No idea wht could cause this -- just raise it to the caller.
        goto error_return;
    }
    
    if ((value < 0) || (value > SEMAPHORE_VALUE_MAX)) {
		PyErr_Format(PyExc_ValueError, 
		             "Attribute 'value' must be between 0 and %ld (SEMAPHORE_VALUE_MAX)", 
		             (long)SEMAPHORE_VALUE_MAX);
        goto error_return;
    }
    
    arg.val = value;
    
    if (-1 == semctl(self->id, 0, SETVAL, arg)) {
        sem_set_error();
        goto error_return;
    }    
    
    return 0;

    error_return:
    return -1;
}


static PyObject *
sem_get_block(Semaphore *self) {
    DPRINTF("op_flags: %x\n", self->op_flags);
    return PyBool_FromLong( (self->op_flags & IPC_NOWAIT) ? 0 : 1);
}


static int
sem_set_block(Semaphore *self, PyObject *py_value)
{
    DPRINTF("op_flags before: %x\n", self->op_flags);
    
    if (PyObject_IsTrue(py_value))
        self->op_flags &= ~IPC_NOWAIT;
    else
        self->op_flags |= IPC_NOWAIT;
        
    DPRINTF("op_flags after: %x\n", self->op_flags);

    return 0;
}


static PyObject *
sem_get_mode(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_MODE);
}


static int
sem_set_mode(Semaphore *self, PyObject *py_value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_MODE, py_value);
}


static PyObject *
sem_get_undo(Semaphore *self) {
    return PyBool_FromLong( (self->op_flags & SEM_UNDO) ? 1 : 0 );
}


static int
sem_set_undo(Semaphore *self, PyObject *py_value)
{
    DPRINTF("op_flags before: %x\n", self->op_flags);
    
    if (PyObject_IsTrue(py_value))
        self->op_flags |= SEM_UNDO;
    else
        self->op_flags &= ~SEM_UNDO;

    DPRINTF("op_flags after: %x\n", self->op_flags);

    return 0;
}


static PyObject *
sem_get_uid(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_UID);
}


static int
sem_set_uid(Semaphore *self, PyObject *py_value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_UID, py_value);
}


static PyObject *
sem_get_gid(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_GID);
}


static int
sem_set_gid(Semaphore *self, PyObject *py_value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_GID, py_value);
}

static PyObject *
sem_get_c_uid(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_CUID);
}

static PyObject *
sem_get_c_gid(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_CGID);
}

static PyObject *
sem_get_last_pid(Semaphore *self) {
    return sem_get_semctl_value(self->id, GETPID);
}

static PyObject *
sem_get_waiting_for_nonzero(Semaphore *self) {
    return sem_get_semctl_value(self->id, GETNCNT);
}

static PyObject *
sem_get_waiting_for_zero(Semaphore *self) {
    return sem_get_semctl_value(self->id, GETZCNT);
}

static PyObject *
sem_get_o_time(Semaphore *self) {
    return sem_get_ipc_perm_value(self->id, SEM_OTIME);
}



/*  

       Shared memory implementation functions
    
*/


static PyObject *
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


static void 
SharedMemory_dealloc(SharedMemory *self) {
    self->ob_type->tp_free((PyObject*)self); 
}

static PyObject *
SharedMemory_new(PyTypeObject *type, PyObject *args, PyObject *kwlist) {
    SharedMemory *self;
    
    self = (SharedMemory *)type->tp_alloc(type, 0);

    return (PyObject *)self; 
}


static int 
SharedMemory_init(SharedMemory *self, PyObject *args, PyObject *keywords) {
    long key;
    int mode = 0600;
    unsigned long size = 0;
    int shmget_flags = 0;
    int shmat_flags = 0;
    char init_character = ' ';
    static char *keyword_list[ ] = {"key", "flags", "mode", "size", "init_character", NULL};
    PyObject *py_size = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "l|iikc", keyword_list, 
                                     &key, &shmget_flags, &mode, &size, 
                                     &init_character))
        goto error_return;
    
    if ((key < 0) || (key > KEY_MAX)) {
        PyErr_Format(PyExc_ValueError, 
                     "Key must be between 0 and %ld (KEY_MAX)", 
                     (long)KEY_MAX);
        goto error_return;
    }

    mode &= 0777;
    shmget_flags &= ~0777;
    
    // When creating a new segment, the default size is PAGE_SIZE.
    if (((shmget_flags & IPC_CREX) == IPC_CREX) && (!size))
        size = PAGE_SIZE;

    DPRINTF("Calling shmget, key=%ld, size=%lu, mode=%o, flags=0x%x\n", 
            key, size, mode, shmget_flags);
    self->id = shmget(key, size, mode | shmget_flags);
        
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
                             key);
            break;

            case ENOENT:
                PyErr_Format(pExistentialException, 
                             "No shared memory exists with the key %ld", key);
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


static PyObject *
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


static PyObject *
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


static PyObject *
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
    static char *keyword_list[ ] = {"byte_count", "offset", NULL};
    
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


static PyObject *
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


static PyObject *
SharedMemory_remove(SharedMemory *self) {
    return shm_remove(self->id);
}


static PyObject *
shm_get_size(SharedMemory *self) {
    return shm_get_value(self->id, SHM_SIZE);
}

static PyObject *
shm_get_address(SharedMemory *self) {
    return PyLong_FromVoidPtr(self->address);
}

static PyObject *
shm_get_attached(SharedMemory *self) {
    if (self->address) 
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
shm_get_last_attach_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_ATTACH_TIME);
}

static PyObject *
shm_get_last_detach_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_DETACH_TIME);
}

static PyObject *
shm_get_last_change_time(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_CHANGE_TIME);
}

static PyObject *
shm_get_creator_pid(SharedMemory *self) {
    return shm_get_value(self->id, SHM_CREATOR_PID);
}

static PyObject *
shm_get_last_pid(SharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_AT_DT_PID);
}

static PyObject *
shm_get_number_attached(SharedMemory *self) {
    return shm_get_value(self->id, SHM_NUMBER_ATTACHED);
}

static PyObject *
shm_get_uid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_UID);
}

static PyObject *
shm_get_cuid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CUID);
}

static PyObject *
shm_get_cgid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CGID);
}

static PyObject *
shm_get_mode(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_MODE);
}

static int
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


static PyObject *
shm_get_gid(SharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_GID);
}

static int
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

static int
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


/* 

        Module methods 

*/

static PyObject *
sysv_ipc_remove_semaphore(PyObject *self, PyObject *args) {
    int id; 

    if (!PyArg_ParseTuple(args, "i", &id))
        goto error_return;

    DPRINTF("removing sem with id %d\n", id);
    if (NULL == sem_get_semctl_value(id, IPC_RMID))
        goto error_return;
    
    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
sysv_ipc_remove_shared_memory(PyObject *self, PyObject *args) {
    int id; 

    if (!PyArg_ParseTuple(args, "i", &id))
        goto error_return;

    return shm_remove(id);
    
    error_return:
    return NULL;    
}


static PyMemberDef Semaphore_members[] = {
    {"id", T_INT, offsetof(Semaphore, id), READONLY, "semaphore id"}, 
    {"key", T_INT, offsetof(Semaphore, key), READONLY, "semaphore key"}, 
    {NULL} /* Sentinel */ 
};


static PyMethodDef Semaphore_methods[] = { 
    {   "P", 
        (PyCFunction)Semaphore_P, 
        METH_KEYWORDS, 
        "Acquire (decrement) the semaphore, waiting if necessary"
    },
    {   "acquire", 
        (PyCFunction)Semaphore_acquire, 
        METH_KEYWORDS, 
        "Acquire (decrement) the semaphore, waiting if necessary"
    },
    {   "V",
        (PyCFunction)Semaphore_V, 
        METH_KEYWORDS, 
        "Release (increment) the semaphore"
    }, 
    {   "release", 
        (PyCFunction)Semaphore_release, 
        METH_KEYWORDS, 
        "Release (increment) the semaphore"
    },
    {   "Z",
        (PyCFunction)Semaphore_Z, 
        METH_KEYWORDS, 
        "Waits until zee zemaphore is zero"
    }, 
    {   "remove",
        (PyCFunction)Semaphore_remove, 
        METH_NOARGS, 
        "Removes (deletes) the semaphore from the system"
    }, 
    {NULL, NULL, 0, NULL} /* Sentinel */ 
};



static PyGetSetDef Semaphore_gets_and_sets[] = { 
    {"value", (getter)sem_get_value, (setter)sem_set_value, "value", NULL}, 
    {"undo", (getter)sem_get_undo, (setter)sem_set_undo, "undo", NULL}, 
    {"block", (getter)sem_get_block, (setter)sem_set_block, "block", NULL}, 
    {"mode", (getter)sem_get_mode, (setter)sem_set_mode, "mode", NULL}, 
    {"uid", (getter)sem_get_uid, (setter)sem_set_uid, "uid", NULL}, 
    {"gid", (getter)sem_get_gid, (setter)sem_set_gid, "gid", NULL}, 
    {"cuid", (getter)sem_get_c_uid, (setter)NULL, "cuid", NULL}, 
    {"cgid", (getter)sem_get_c_gid, (setter)NULL, "cgid", NULL}, 
    {"last_pid", (getter)sem_get_last_pid, (setter)NULL, "last_pid", NULL}, 
    {"waiting_for_nonzero", (getter)sem_get_waiting_for_nonzero, (setter)NULL, "waiting_for_nonzero", NULL}, 
    {"waiting_for_zero", (getter)sem_get_waiting_for_zero, (setter)NULL, "waiting_for_zero", NULL},
    {"o_time", (getter)sem_get_o_time, (setter)NULL, "o_time", NULL},
    {NULL} /* Sentinel */ 
};




static PyTypeObject SemaphoreType = {
    PyObject_HEAD_INIT(NULL)
    0, /*ob_size*/
    "sysv_ipc.Semaphore", /*tp_name*/
    sizeof(Semaphore), /*tp_basicsize*/
    0, /*tp_itemsize*/
    (destructor)Semaphore_dealloc, /*tp_dealloc*/
    0, /*tp_print*/
    0, /*tp_getattr*/
    0, /*tp_setattr*/
    0, /*tp_compare*/
    0, /*tp_repr*/
    0, /*tp_as_number*/
    0, /*tp_as_sequence*/
    0, /*tp_as_mapping*/
    0, /*tp_hash */
    0, /*tp_call*/
    0, /*tp_str*/
    0, /*tp_getattro*/
    0, /*tp_setattro*/
    0, /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "System V semaphore objects", /* tp_doc */
    0, /* tp_traverse */ 
    0, /* tp_clear */ 
    0, /* tp_richcompare */ 
    0, /* tp_weaklistoffset */ 
    0, /* tp_iter */ 
    0, /* tp_iternext */ 
    Semaphore_methods, /* tp_methods */ 
    Semaphore_members, /* tp_members */ 
    Semaphore_gets_and_sets, /* tp_getset */ 
    0, /* tp_base */ 
    0, /* tp_dict */ 
    0, /* tp_descr_get */ 
    0, /* tp_descr_set */ 
    0, /* tp_dictoffset */ 
    (initproc)Semaphore_init, /* tp_init */ 
    0, /* tp_alloc */ 
    Semaphore_new, /* tp_new */ 
};



static PyMemberDef SharedMemory_members[] = {
    {"id", T_INT, offsetof(SharedMemory, id), READONLY, "semaphore id"}, 
    {"key", T_INT, offsetof(SharedMemory, key), READONLY, "semaphore key"}, 
    {NULL} /* Sentinel */ 
};


static PyMethodDef SharedMemory_methods[] = { 
    {   "read",
        (PyCFunction)SharedMemory_read, 
        METH_KEYWORDS, 
        "Read n bytes from the shared memory at the given offset into a Python string"
    },
    {   "write",
        (PyCFunction)SharedMemory_write, 
        METH_KEYWORDS, 
        "Write the string to the shared memory at the offset given"
    }, 
    {   "remove",
        (PyCFunction)SharedMemory_remove, 
        METH_NOARGS, 
        "Removes (deletes) the shared memory from the system"
    }, 
    {   "attach",
        (PyCFunction)SharedMemory_attach, 
        METH_VARARGS, 
        "Attaches the shared memory"
    }, 
    {   "detach",
        (PyCFunction)SharedMemory_detach, 
        METH_NOARGS, 
        "Detaches the shared memory"
    }, 
    {NULL, NULL, 0, NULL} /* Sentinel */ 
};



static PyGetSetDef SharedMemory_gets_and_sets[] = { 
    {"size", (getter)shm_get_size, (setter)NULL, "size", NULL}, 
    {"address", (getter)shm_get_address, (setter)NULL, "address", NULL}, 
    {"attached", (getter)shm_get_attached, (setter)NULL, "attached", NULL}, 
    {"last_attach_time", (getter)shm_get_last_attach_time, (setter)NULL, "last_attach_time", NULL}, 
    {"last_detach_time", (getter)shm_get_last_detach_time, (setter)NULL, "last_detach_time", NULL}, 
    {"last_change_time", (getter)shm_get_last_change_time, (setter)NULL, "last_change_time", NULL}, 
    {"creator_pid", (getter)shm_get_creator_pid, (setter)NULL, "creator_pid", NULL}, 
    {"last_pid", (getter)shm_get_last_pid, (setter)NULL, "last_pid", NULL}, 
    {"number_attached", (getter)shm_get_number_attached, (setter)NULL, "number_attached", NULL}, 
    {"uid", (getter)shm_get_uid, (setter)shm_set_uid, "uid", NULL}, 
    {"gid", (getter)shm_get_gid, (setter)shm_set_gid, "gid", NULL}, 
    {"cuid", (getter)shm_get_cuid, (setter)NULL, "cuid", NULL}, 
    {"cgid", (getter)shm_get_cgid, (setter)NULL, "cgid", NULL}, 
    {"mode", (getter)shm_get_mode, (setter)shm_set_mode, "mode", NULL}, 
    {NULL} /* Sentinel */ 
};



static PyTypeObject SharedMemoryType = {
    PyObject_HEAD_INIT(NULL)
    0, /*ob_size*/
    "sysv_ipc.SharedMemory", /*tp_name*/
    sizeof(SharedMemory), /*tp_basicsize*/
    0, /*tp_itemsize*/
    (destructor)SharedMemory_dealloc, /*tp_dealloc*/
    0, /*tp_print*/
    0, /*tp_getattr*/
    0, /*tp_setattr*/
    0, /*tp_compare*/
    0, /*tp_repr*/
    0, /*tp_as_number*/
    0, /*tp_as_sequence*/
    0, /*tp_as_mapping*/
    0, /*tp_hash */
    0, /*tp_call*/
    0, /*tp_str*/
    0, /*tp_getattro*/
    0, /*tp_setattro*/
    0, /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /*tp_flags*/
    "System V shared memory objects", /* tp_doc */
    0, /* tp_traverse */ 
    0, /* tp_clear */ 
    0, /* tp_richcompare */ 
    0, /* tp_weaklistoffset */ 
    0, /* tp_iter */ 
    0, /* tp_iternext */ 
    SharedMemory_methods, /* tp_methods */ 
    SharedMemory_members, /* tp_members */ 
    SharedMemory_gets_and_sets, /* tp_getset */ 
    0, /* tp_base */ 
    0, /* tp_dict */ 
    0, /* tp_descr_get */ 
    0, /* tp_descr_set */ 
    0, /* tp_dictoffset */ 
    (initproc)SharedMemory_init, /* tp_init */ 
    0, /* tp_alloc */ 
    SharedMemory_new, /* tp_new */ 
};


/*

    Module level stuff
    
*/


static PyMethodDef module_methods[ ] = { 
    {   "remove_semaphore", 
        (PyCFunction)sysv_ipc_remove_semaphore,
        METH_VARARGS,
        "Remove (delete) the semaphore identified by id"
    },
    {   "remove_shared_memory", 
        (PyCFunction)sysv_ipc_remove_shared_memory, 
        METH_VARARGS,
        "Remove shared memory"
    },
    {NULL} /* Sentinel */ 
};



/* Module init function */
PyMODINIT_FUNC 
initsysv_ipc(void) {
    PyObject *m;
    PyObject *module_dict;

    if (PyType_Ready(&SemaphoreType) < 0)
        goto error_return;
 
    if (PyType_Ready(&SharedMemoryType) < 0)
        goto error_return;
 
    m = Py_InitModule3("sysv_ipc", module_methods, "System V IPC module");

    if (m == NULL) 
        goto error_return;

    PyModule_AddIntConstant(m, "PAGE_SIZE", PAGE_SIZE);
    PyModule_AddIntConstant(m, "KEY_MAX", KEY_MAX);    
    PyModule_AddIntConstant(m, "SEMAPHORE_VALUE_MAX", SEMAPHORE_VALUE_MAX);
    PyModule_AddIntConstant(m, "IPC_CREAT", IPC_CREAT);
    PyModule_AddIntConstant(m, "IPC_EXCL", IPC_EXCL);
    PyModule_AddIntConstant(m, "IPC_CREX", IPC_CREX);
    PyModule_AddIntConstant(m, "IPC_PRIVATE", IPC_PRIVATE);
    PyModule_AddIntConstant(m, "SHM_RND", SHM_RND);
    PyModule_AddIntConstant(m, "SHM_RDONLY", SHM_RDONLY);

#ifdef SEMTIMEDOP_EXISTS
    Py_INCREF(Py_True);
    PyModule_AddObject(m, "SEMAPHORE_TIMEOUT_SUPPORTED", Py_True);
#else
    Py_INCREF(Py_False);
    PyModule_AddObject(m, "SEMAPHORE_TIMEOUT_SUPPORTED", Py_False);
#endif

    // These flags are Linux-specific.
#ifdef SHM_HUGETLB
    PyModule_AddIntConstant(m, "SHM_HUGETLB", SHM_HUGETLB);
#endif    
#ifdef SHM_NORESERVE
    PyModule_AddIntConstant(m, "SHM_NORESERVE", SHM_NORESERVE);
#endif    
#ifdef SHM_REMAP
    PyModule_AddIntConstant(m, "SHM_REMAP", SHM_REMAP);
#endif    

    Py_INCREF(&SemaphoreType);
    PyModule_AddObject(m, "Semaphore", (PyObject *)&SemaphoreType);

    Py_INCREF(&SharedMemoryType);
    PyModule_AddObject(m, "SharedMemory", (PyObject *)&SharedMemoryType);
    
    
    // Exceptions
    if (!(module_dict = PyModule_GetDict(m)))
        goto error_return;

    if (!(pBaseException = PyErr_NewException("sysv_ipc.Error", NULL, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "Error", pBaseException);

    if (!(pInternalException = PyErr_NewException("sysv_ipc.InternalError", NULL, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "InternalError", pInternalException);
    
    if (!(pPermissionsException = PyErr_NewException("sysv_ipc.PermissionsError", pBaseException, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "PermissionsError", pPermissionsException);
    
    if (!(pExistentialException = PyErr_NewException("sysv_ipc.ExistentialError", pBaseException, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "ExistentialError", pExistentialException);
    
    if (!(pBusyException = PyErr_NewException("sysv_ipc.BusyError", pBaseException, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "BusyError", pBusyException);
    
    if (!(pNotAttachedException = PyErr_NewException("sysv_ipc.NotAttachedError", pBaseException, NULL)))
        goto error_return;
    else
        PyDict_SetItemString(module_dict, "NotAttachedError", pNotAttachedException);


    return;

    error_return:
    return;
}




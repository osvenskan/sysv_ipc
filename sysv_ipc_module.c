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

// _XOPEN_SOURCE is necessary for clean compiles under Ubuntu; also gives me
// the newer definition of the ipc_perm struct under Darwin. Probably has
// beneficial effects elsewhere too.
#define _XOPEN_SOURCE 600

#include "Python.h"

#include "structmember.h"

// for memset
#include <string.h>

// For sysconf/getpagesize
#include <sys/mman.h>
#include <sys/stat.h>

// For the math surrounding timeouts for semtimedop()
#include <math.h>

// Code for determining page size swiped from Python's mmapmodule.c
#if defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
static int
my_getpagesize(void)
{
	return sysconf(_SC_PAGESIZE);
}
#else
#define my_getpagesize getpagesize
#endif


#include <sys/ipc.h>		/* for system's IPC_xxx definitions */
#include <sys/shm.h>		/* for shmget, shmat, shmdt, shmctl */
#include <sys/sem.h>		/* for semget, semctl, semop */

/* The name of this member varies and is sniffed out by setup.py. */
#if defined(ZERO_UNDERSCORE_KEY)
#define IPC_PERM_KEY_NAME     key
#elif defined(ONE_UNDERSCORE_KEY)
#define IPC_PERM_KEY_NAME    _key
#elif defined(TWO_UNDERSCORE_KEY)
#define IPC_PERM_KEY_NAME   __key
#endif

/* The name of this member varies and is sniffed out by setup.py. */
#if defined(ZERO_UNDERSCORE_KEY)
#define IPC_PERM_SEQ_NAME     seq
#elif defined(ONE_UNDERSCORE_KEY)
#define IPC_PERM_SEQ_NAME    _seq
#elif defined(TWO_UNDERSCORE_KEY)
#define IPC_PERM_SEQ_NAME   __seq
#endif

enum GET_SET_IDENTIFIERS {
    IPC_PERM_KEY = 1,
    IPC_PERM_UID,
    IPC_PERM_GID,
    IPC_PERM_CUID,
    IPC_PERM_CGID,
    IPC_PERM_MODE,
    IPC_PERM_SEQ,
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


/*

      Exceptions for this module
        
*/

static PyObject *pSysVIpcException;
static PyObject *pSysVIpcInternalException;
static PyObject *pSysVIpcPermissionsException;
static PyObject *pSysVIpcExistentialException;
static PyObject *pSysVIpcBusyException;
static PyObject *pSysVIpcNotAttachedException;


typedef struct {
    PyObject_HEAD
    int id;
    void *address;
} SysVSharedMemory;

typedef struct {
    PyObject_HEAD
    int id;
    key_t key;
    short op_flags;
} SysVSemaphore;

#define ONE_BILLION 1000000000

//#define SYSV_IPC_DEBUG  

#ifdef SYSV_IPC_DEBUG
#define DPRINTF(fmt, args...) fprintf(stderr, "+++ " fmt, ## args)
#else
#define DPRINTF(fmt, args...)
#endif

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

/* Utility functions */

static PyObject *
py_int_or_long_from_long(long value) {
    if (value > PyInt_GetMax())
        return PyLong_FromLong(value);
    else
        return PyInt_FromLong(value);
}


void sem_set_error(PyObject *timeout) {
    char msg[1024];
    
    switch (errno) {
        case ENOENT:
        case EINVAL:
            //sprintf(msg, "No semaphore exists with the specified key (%ld)", (long)key);
            sprintf(msg, "No semaphore exists with the specified key");
            PyErr_SetString(pSysVIpcExistentialException, msg);
        break;

        case EEXIST:
            //sprintf(msg, "A semaphore with the specified key (%ld) already exists", (long)key);
            sprintf(msg, "A semaphore with the specified key already exists");
            PyErr_SetString(pSysVIpcExistentialException, msg);
        break;

        case EACCES:
            PyErr_SetString(pSysVIpcPermissionsException, "No permission to access this semaphore");
        break;
        
        case ERANGE:
            PyErr_SetString(PyExc_ValueError, "The semaphore's value must remain between 0 and SEMVMX");
        break;
        
        case EAGAIN:
            PyErr_SetString(pSysVIpcBusyException, "Semaphore is busy");
            // if ((NULL == timeout) || (Py_None == timeout))
            //     PyErr_SetString(pSysVIpcBusyException, "Semaphore is busy");
            // else
            //     PyErr_SetString(pSysVIpcTimeoutException, "Timeout expired");
        break;

        case EIDRM:
            PyErr_SetString(pSysVIpcExistentialException, "The semaphore was removed");
        break;
        
        case EINTR:
            PyErr_SetString(pSysVIpcException, "Signaled while waiting");
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
sem_perform_semop(enum SEMOP_TYPE op_type, SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    struct sembuf op[1];
    double simple_timeout = 0.0;
    PyObject *pTimeout = Py_None;
    int delta = 0;
    int arg_is_ok = 0;
    int rc = 0;
    char msg[1024];
    static char *keyword_list[3][3] = {
                    {"timeout", "delta", NULL},     // P
                    {"delta", NULL},                // V
                    {"timeout", NULL}               // Z
                };

    // op_type is P, V or Z corresponding to the 3 SysVSemaphore methods 
    // that call that call semop(). 
    
    switch (op_type) {
        case SEMOP_P:
            delta = -1;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|Oi",
                                             keyword_list[SEMOP_P], 
                                             &pTimeout, &delta);
            if (delta > 0)
                delta = -abs(delta);
        break;
            
        case SEMOP_V:
            delta = 1;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|i", 
                                             keyword_list[SEMOP_V], &delta);
            if (delta < 0)
                delta = abs(delta);
        break;
        
        case SEMOP_Z:
            delta = 0;
            rc = PyArg_ParseTupleAndKeywords(args, keywords, "|O",
                                             keyword_list[SEMOP_Z], 
                                             &pTimeout);
        break;
        
        default:
            sprintf(msg, "Bad op_type (%d)", op_type);
            PyErr_SetString(pSysVIpcInternalException, msg); 
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

    // The timeout can be None or any Python numeric type (float, int, long).
    if (pTimeout == Py_None) {
        arg_is_ok = 1;
    }
    else if (PyFloat_Check(pTimeout)) {
        arg_is_ok = 1;
        simple_timeout = PyFloat_AsDouble(pTimeout);
    }
    else if (PyInt_Check(pTimeout)) {
        arg_is_ok = 1;
        simple_timeout = (double)PyInt_AsLong(pTimeout);
    }
    else if (PyLong_Check(pTimeout)) {
        arg_is_ok = 1;
        simple_timeout = (double)PyLong_AsLong(pTimeout);
    }

    if (!arg_is_ok) {
        PyErr_SetString(PyExc_TypeError, "timeout must be numeric or None");
        goto error_return;
    }

    // The arg is verified to be OK at this point; now let's figure out 
    // what it means.
    if (pTimeout != Py_None) {
        // simple_timeout contains the numeric argument passed by the caller.
        if (simple_timeout) {
            // The caller wants a timed wait so blocking mode must be ON.
            op[0].sem_flg &= ~IPC_NOWAIT;
        }
        else {
            // The caller wants an instant timeout, i.e. non-blocking mode.
            op[0].sem_flg |= IPC_NOWAIT;
        }
    }
    // else 
        // Timeout == None implies deference to the block attribute, so 
        // I don't need to alter op[0].sem_flg.


    Py_BEGIN_ALLOW_THREADS    
#ifdef SEMTIMEDOP_EXISTS
    // Call semtimedop() if appropriate, otherwise call semop()
    if (simple_timeout) {
        struct timespec complex_timeout;
        
        // Note the difference between this and POSIX semaphores. System V sempahores
        // expect tv_sec to represent a delta from the current time whereas POSIX
        // semaphores expect an absolute value.
        complex_timeout.tv_sec = (time_t)floor(simple_timeout);
        complex_timeout.tv_nsec = (long)((simple_timeout - floor(simple_timeout)) * ONE_BILLION);
    
        DPRINTF("calling semtimedop on id %d, op.sem_op = %d, op.flags=%x\n", self->id, op[0].sem_op, op[0].sem_flg);
        DPRINTF("timeout.tv_sec = %ld; timeout.tv_nsec = %ld\n", complex_timeout.tv_sec, complex_timeout.tv_nsec);
        rc = semtimedop(self->id, op, (size_t)1, &complex_timeout);
    }
    else {
        DPRINTF("calling semop on id %d, op.sem_op = %d, op.flags=%x\n", self->id, op[0].sem_op, op[0].sem_flg);
        rc = semop(self->id, op, (size_t)1);
    }
#else 
    // no support for semtimedop(), always call semop() instead.
    DPRINTF("calling semop on id %d, op.sem_op = %d, op.flags=%x\n", self->id, op[0].sem_op, op[0].sem_flg);
    rc = semop(self->id, op, (size_t)1);
#endif
    Py_END_ALLOW_THREADS
    if (rc == -1) {
        sem_set_error(pTimeout);
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
        sem_set_error(NULL);
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
    PyObject *pReturn = NULL;
    char msg[1024];
    
    arg.buf = &sem_info;
    
    // Here I get the values currently associated with the semaphore. 
    if (-1 == semctl(id, 0, IPC_STAT, arg)) {
        sem_set_error(NULL);
        goto error_return;
    }
    
    // Some of these values could theoretically overflow a Python int while others
    // (such as mode) are guaranteed to be int-safe.
    switch (field) {
        case IPC_PERM_KEY:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_perm.IPC_PERM_KEY_NAME);
        break;

        case IPC_PERM_UID:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_perm.uid);
        break;

        case IPC_PERM_GID:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_perm.gid);
        break;
        
        case IPC_PERM_CUID:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_perm.cuid);
        break;
        
        case IPC_PERM_CGID:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_perm.cgid);
        break;
        
        case IPC_PERM_MODE:
            pReturn = PyInt_FromLong((long)sem_info.sem_perm.mode);
        break;
        
        case IPC_PERM_SEQ:
            pReturn = PyInt_FromLong((long)sem_info.sem_perm.IPC_PERM_SEQ_NAME);
        break;
        
        // This isn't an ipc_perm value but it fits here anyway.
        case SEM_OTIME:
            pReturn = py_int_or_long_from_long((long)sem_info.sem_otime);
        break;
        
        default:
            sprintf(msg, "Bad field %d passed to sem_get_ipc_perm_value", field);
            PyErr_SetString(pSysVIpcInternalException, msg); 
        break;
    }
    
    return pReturn;

    error_return:
    return NULL;    
}


static int
sem_set_ipc_perm_value(int id, enum GET_SET_IDENTIFIERS field, PyObject *value) {
    struct semid_ds sem_info;
    union semun arg;
    char msg[1024];
    
    arg.buf = &sem_info;

    if (!PyInt_Check(value)) {
        PyErr_Format(PyExc_TypeError, "attribute must be an integer");
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
        sem_set_error(NULL);
        goto error_return;
    }
    
    switch (field) {
        case IPC_PERM_UID:
            sem_info.sem_perm.uid = (uid_t)PyInt_AsLong(value);
        break;
        
        case IPC_PERM_GID:
            sem_info.sem_perm.gid = (gid_t)PyInt_AsLong(value);
        break;
        
        case IPC_PERM_MODE:
            sem_info.sem_perm.mode = (unsigned short)PyInt_AsLong(value);
        break;

        default:
            sprintf(msg, "Bad field %d passed to sem_set_ipc_perm_value", field);
            PyErr_SetString(pSysVIpcInternalException, msg); 
        break;
    }
    
    if (-1 == semctl(id, 0, IPC_SET, arg)) {
        sem_set_error(NULL);
        goto error_return;
    }
    
    return 0;

    error_return:
    return -1;
}

static void 
SysVSemaphore_dealloc(SysVSemaphore *self) {
    self->ob_type->tp_free((PyObject*)self); 
}

static PyObject *
SysVSemaphore_new(PyTypeObject *type, PyObject *args, PyObject *keywords) {
    SysVSemaphore *self;
    
    self = (SysVSemaphore *)type->tp_alloc(type, 0);

    return (PyObject *)self; 
}


static int 
SysVSemaphore_init(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    key_t key;
    int mode = 0600;
    unsigned int initial_value = 0;
    int flags = 0;
//    struct semid_ds sem_info;
    union semun arg;
    static char *keyword_list[ ] = {"key", "flags", "mode", "initial_value", NULL};
    
    //SysVSemaphore(key, [flags = 0, [mode = 0600, [initial_value = 0]]])
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "l|iiI", keyword_list, 
                                        &key, &flags, &mode, &initial_value))
        goto error_return;
        
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
        sem_set_error(NULL);
        goto error_return;
    }

    if (flags & (IPC_CREAT | IPC_EXCL)) {
        DPRINTF("setting initial value to %d\n", initial_value);
        arg.val = initial_value;

        if (-1 == semctl(self->id, 0, SETVAL, arg)) {
            sem_set_error(NULL);
            goto error_return;
        }
        
        /* Code below commented out because it is a questionable design choice. 
           I don't see how checking otime fixes the race condition. AFAICT it 
           only shrinks the window because checking otime and setting the 
           semaphore's value are not atomic operations.
           
           Consider the following:
           1) Process 1 (P1) creates the semaphore by calling semget(IPC_CREAT)
           2) Process 2 (P2) gets a handle to the semaphore by calling 
              semget(IPC_CREAT)
           3) P2 checks the otime, finds it is zero.
           4) P1 checks the otime, finds it is zero.
           5) P1 sets the semaphore's value to 5. 
           6) P1 acquires the semaphore by calling semop() which changes the
              value to 4. That also sets otime to a non-zero value, but that's
              irrelevant now because P2 has already checked otime and thinks 
              it is zero.
           7) P2 sets the semaphore's value to 5. 
           
        */
           
           
        
        // // Tricky business. When IPC_EXCL is not set, I don't know if I've 
        // // just created a new semaphore or gotten a handle to an existing
        // // one. If the former is true, I need to initialize the value. If
        // // the latter is true, I must *not* initialize it.
        // // The recommended way to tell is to examine the sem_otime value
        // // of the semid_ds, so I query that here if necessary.
        // if (!(flags & IPC_EXCL)) {
        //     arg.buf = &sem_info;
        //     
        //     if (-1 == semctl(self->id, 0, IPC_STAT, arg)) {
        //         sem_set_error(NULL);
        //         goto error_return;
        //     }
        //     
        //     DPRINTF("sem_otime is %ld\n", (long)sem_info.sem_otime);
        // }
        // 
        // // Note the use of C's short-circuit logic. sem_info is not necessarily 
        // // initialized at this point, but if IPC_EXCL is set, sem_info won't be
        // // examined in the test below. 
        // if ((flags & IPC_EXCL) || (0 == sem_info.sem_otime)) {
        //     DPRINTF("setting initial value to %d\n", initial_value);
        //     arg.val = initial_value;
        // 
        //     if (-1 == semctl(self->id, 0, SETVAL, arg)) {
        //         sem_set_error(NULL);
        //         goto error_return;
        //     }
        // }
    }
    
    return 0; 
    
    error_return:
    return -1;
}


static PyObject *
SysVSemaphore_P(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_P, self, args, keywords);
}


static PyObject *
SysVSemaphore_acquire(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    return SysVSemaphore_P(self, args, keywords);
}


static PyObject *
SysVSemaphore_V(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_V, self, args, keywords);
}


static PyObject *
SysVSemaphore_release(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    return SysVSemaphore_V(self, args, keywords);
}


static PyObject *
SysVSemaphore_Z(SysVSemaphore *self, PyObject *args, PyObject *keywords) {
    return sem_perform_semop(SEMOP_Z, self, args, keywords);
}


static PyObject *
SysVSemaphore_remove(SysVSemaphore *self) {
    if (NULL == sem_get_semctl_value(self->id, IPC_RMID))
        goto error_return;
    
    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
sem_get_key(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_KEY);
}

static PyObject *
sem_get_value(SysVSemaphore *self) {
    return sem_get_semctl_value(self->id, GETVAL);
}


static int
sem_set_value(SysVSemaphore *self, PyObject *value)
{
    union semun arg;

    if (!PyInt_Check(value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'value' must be an integer");
        goto error_return;
    }

    arg.val = (int)PyInt_AsLong(value);

    if ((-1 == arg.val) && PyErr_Occurred()) {
        // No idea wht could cause this -- just raise it to the caller.
        goto error_return;
    }
    
    if (-1 == semctl(self->id, 0, SETVAL, arg)) {
        sem_set_error(NULL);
        goto error_return;
    }    
    
    return 0;

    error_return:
    return -1;
}


static PyObject *
sem_get_block(SysVSemaphore *self) {
    DPRINTF("op_flags: %x\n", self->op_flags);
    return PyBool_FromLong( (self->op_flags & IPC_NOWAIT) ? 0 : 1);
}


static int
sem_set_block(SysVSemaphore *self, PyObject *value)
{
    DPRINTF("op_flags before: %x\n", self->op_flags);
    
    if (PyObject_IsTrue(value))
        self->op_flags &= ~IPC_NOWAIT;
    else
        self->op_flags |= IPC_NOWAIT;
        
    DPRINTF("op_flags after: %x\n", self->op_flags);

    return 0;
}


static PyObject *
sem_get_mode(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_MODE);
}


static int
sem_set_mode(SysVSemaphore *self, PyObject *value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_MODE, value);
}


static PyObject *
sem_get_undo(SysVSemaphore *self) {
    return PyBool_FromLong( (self->op_flags & SEM_UNDO) ? 1 : 0 );
}


static int
sem_set_undo(SysVSemaphore *self, PyObject *value)
{
    DPRINTF("op_flags before: %x\n", self->op_flags);
    
    if (PyObject_IsTrue(value))
        self->op_flags |= SEM_UNDO;
    else
        self->op_flags &= ~SEM_UNDO;

    DPRINTF("op_flags after: %x\n", self->op_flags);

    return 0;
}


static PyObject *
sem_get_uid(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_UID);
}


static int
sem_set_uid(SysVSemaphore *self, PyObject *value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_UID, value);
}


static PyObject *
sem_get_gid(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_GID);
}


static int
sem_set_gid(SysVSemaphore *self, PyObject *value) {
    return sem_set_ipc_perm_value(self->id, IPC_PERM_GID, value);
}

static PyObject *
sem_get_c_uid(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_CUID);
}

static PyObject *
sem_get_c_gid(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, IPC_PERM_CGID);
}

static PyObject *
sem_get_last_pid(SysVSemaphore *self) {
    return sem_get_semctl_value(self->id, GETPID);
}

static PyObject *
sem_get_waiting_for_nonzero(SysVSemaphore *self) {
    return sem_get_semctl_value(self->id, GETNCNT);
}

static PyObject *
sem_get_waiting_for_zero(SysVSemaphore *self) {
    return sem_get_semctl_value(self->id, GETZCNT);
}

static PyObject *
sem_get_o_time(SysVSemaphore *self) {
    return sem_get_ipc_perm_value(self->id, SEM_OTIME);
}



/*  

       Shared memory implementation functions
    
*/

static PyObject *
shm_remove(int shared_memory_id) {
    struct shmid_ds shm_info;

    DPRINTF("removing shm with id %d\n", shared_memory_id);
    if (-1 == shmctl(shared_memory_id, IPC_RMID, &shm_info))
        goto error_return;
    
    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
shm_get_value(int shared_memory_id, enum GET_SET_IDENTIFIERS field) {
    struct shmid_ds shm_info;
    PyObject *pReturn = NULL;
    char msg[1024];

    DPRINTF("Calling shmctl(...IPC_STAT...), field = %d\n", field);
    if (-1 == shmctl(shared_memory_id, IPC_STAT, &shm_info)) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }

    switch (field) {
        case SHM_SIZE:
            pReturn = py_int_or_long_from_long((unsigned long)shm_info.shm_segsz);
        break;

        case SHM_LAST_ATTACH_TIME:
            pReturn = py_int_or_long_from_long((unsigned long)shm_info.shm_atime);
        break;
        
        case SHM_LAST_DETACH_TIME:
            pReturn = py_int_or_long_from_long((unsigned long)shm_info.shm_dtime);
        break;
        
        case SHM_LAST_CHANGE_TIME:
            pReturn = py_int_or_long_from_long((unsigned long)shm_info.shm_ctime);
        break;

        case SHM_CREATOR_PID:
            pReturn = PyInt_FromLong((long)shm_info.shm_cpid);
        break;

        case SHM_LAST_AT_DT_PID:
            pReturn = PyInt_FromLong((long)shm_info.shm_lpid);
        break;

        case SHM_NUMBER_ATTACHED:
            pReturn = PyInt_FromLong((long)shm_info.shm_nattch);
        break;

        case IPC_PERM_KEY:
            pReturn = py_int_or_long_from_long((long)shm_info.shm_perm.IPC_PERM_KEY_NAME);
        break;

        case IPC_PERM_UID:
            pReturn = py_int_or_long_from_long((long)shm_info.shm_perm.uid);
        break;

        case IPC_PERM_GID:
            pReturn = py_int_or_long_from_long((long)shm_info.shm_perm.gid);
        break;

        case IPC_PERM_CUID:
            pReturn = py_int_or_long_from_long((long)shm_info.shm_perm.cuid);
        break;

        case IPC_PERM_CGID:
            pReturn = py_int_or_long_from_long((long)shm_info.shm_perm.cgid);
        break;

        case IPC_PERM_MODE:
            pReturn = PyInt_FromLong((long)shm_info.shm_perm.mode);
        break;

        case IPC_PERM_SEQ:
            pReturn = PyInt_FromLong((long)shm_info.shm_perm.IPC_PERM_SEQ_NAME);
        break;

        default:
            sprintf(msg, "Bad field %d passed to shm_get_value", field);
            PyErr_SetString(pSysVIpcInternalException, msg); 
        break;
    }
    
    return pReturn;

    error_return:
    return NULL;    
}


static int
shm_set_ipc_perm_value(int id, enum GET_SET_IDENTIFIERS field, int value) {
    // Should value be a union of uid_t/gid_t/unsigned short?
    
    struct shmid_ds shm_info;
    char msg[1024];

    if (-1 == shmctl(id, IPC_STAT, &shm_info)) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }
    
    switch (field) {
        case IPC_PERM_UID:
            shm_info.shm_perm.uid = (uid_t)value;
        break;
        
        case IPC_PERM_GID:
            shm_info.shm_perm.gid = (gid_t)value;
        break;
        
        case IPC_PERM_MODE:
            shm_info.shm_perm.mode = (unsigned short)value;
        break;

        default:
            sprintf(msg, "Bad field %d passed to shm_set_ipc_perm_value", field);
            PyErr_SetString(pSysVIpcInternalException, msg);
        break;
    }
    
    if (-1 == shmctl(id, IPC_SET, &shm_info)) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }
    
    return 0;

    error_return:
    return -1;
}


static void 
SysVSharedMemory_dealloc(SysVSharedMemory *self) {
    self->ob_type->tp_free((PyObject*)self); 
}

static PyObject *
SysVSharedMemory_new(PyTypeObject *type, PyObject *args, PyObject *kwlist) {
    SysVSharedMemory *self;
    
    self = (SysVSharedMemory *)type->tp_alloc(type, 0);

    return (PyObject *)self; 
}


static int 
SysVSharedMemory_init(SysVSharedMemory *self, PyObject *args, PyObject *keywords) {
    key_t key;
    int mode = 0600;
    unsigned long size = my_getpagesize();
    int flags = 0;
    char init_character = ' ';
    static char *keyword_list[ ] = {"key", "flags", "mode", "size", "init_character", NULL};
    PyObject *pSize = NULL;
    
    if (!PyArg_ParseTupleAndKeywords(args, keywords, "l|iilc", keyword_list, 
                                        &key, &flags, &mode, &size, &init_character))
        goto error_return;
    
    mode &= 0777;
    flags &= ~0777;
        
    DPRINTF("Calling shmget, key=%ld, size=%ld, mode=%o, flags=%x\n", (long)key, size, mode, flags);
    self->id = shmget(key, size, mode | flags);
        
    DPRINTF("id == %d\n", self->id);
    
    if (self->id == -1) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }

    DPRINTF("attaching memory with id %d\n", self->id);
    self->address = shmat(self->id, NULL, 0);
    
    if ( (void *)-1 == self->address) {
        self->address = NULL;
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }
    
    if (flags & (IPC_CREAT | IPC_EXCL)) {
        // Initialize the memory.
        pSize = shm_get_value(self->id, SHM_SIZE);
    
        if (!pSize)
            goto error_return;
        else {
            if (PyInt_Check(pSize)) 
                size = PyInt_AsLong(pSize);
            else
                size = PyLong_AsLong(pSize);

            DPRINTF("memsetting address %p to %ld bytes of %c\n", self->address, size, init_character);
            memset(self->address, init_character, size);
    
            Py_DECREF(pSize);
        }
    }
    
    return 0;
    
    error_return:
    return -1;
}


static PyObject *
SysVSharedMemory_attach(SysVSharedMemory *self, PyObject *args) {
    PyObject *long_address = NULL;
    void *address = NULL;
    int flags = 0;
    
    if (!PyArg_ParseTuple(args, "|Oi", &long_address, &flags))
        goto error_return;

    if ((!long_address) || (long_address == Py_None))
        address = NULL;
    else {
        if (PyLong_Check(long_address))
            address = PyLong_AsVoidPtr(long_address);
        else
            PyErr_SetString(PyExc_TypeError, "address must be a long");
    }
    
    self->address = shmat(self->id, address, flags);
    
    if ((void *)-1 == self->address) {
        self->address = NULL;
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }

    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
SysVSharedMemory_detach(SysVSharedMemory *self) {
    if (-1 == shmdt(self->address)) {
        self->address = NULL;
        PyErr_SetFromErrno(PyExc_OSError);
        goto error_return;
    }

    self->address = NULL;

    Py_RETURN_NONE;
    
    error_return:
    return NULL;    
}


static PyObject *
SysVSharedMemory_read(SysVSharedMemory *self, PyObject *args, PyObject *keywords) {
    Py_ssize_t byte_count = 0;
    Py_ssize_t offset = 0;
    PyObject *pSize;
    size_t size;
    static char *keyword_list[ ] = {"byte_count", "offset", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|nn", keyword_list, 
                                     &byte_count, &offset))
        goto error_return;
        
    if (self->address == NULL) {
        PyErr_SetString(pSysVIpcNotAttachedException, "Read attempt on unattached memory segment");
        goto error_return;
    }

    pSize = shm_get_value(self->id, SHM_SIZE);
    if (!pSize) 
        goto error_return;
    else {
        size = (size_t)PyInt_AsLong(pSize);
        Py_DECREF(pSize);
    }

    DPRINTF("offset = %ld, byte_count = %ld, size = %ld\n", (long)offset, (long)byte_count, (long)size);
    
    if ((offset < 0) || (offset >= size)) {
        PyErr_SetString(PyExc_ValueError, "Offset must be between zero and the segment size");
        goto error_return;
    }
    
    // If the caller didn't specify a byte count or specified one that would
    // read past the end of the segment, return everything from the offset to
    // the end of the segment.
    if ((!byte_count) || (offset + byte_count > size))
        byte_count = size - offset;

    return PyString_FromStringAndSize((const char *)(self->address + offset), byte_count);
    
    error_return:
    return NULL;
}


static PyObject *
SysVSharedMemory_write(SysVSharedMemory *self, PyObject *args) {
    const char *buffer;
    Py_ssize_t byte_count;
    Py_ssize_t offset = 0;
    PyObject *pSize;
    size_t size;

    if (!PyArg_ParseTuple(args, "s#|n", &buffer, &byte_count, &offset))
        goto error_return;
	    
    if (self->address == NULL) {
        PyErr_SetString(pSysVIpcNotAttachedException, "Write attempt on unattached memory segment");
        goto error_return;
    }
    
    pSize = shm_get_value(self->id, SHM_SIZE);
    if (!pSize) 
        goto error_return;
    else {
        size = (size_t)PyInt_AsLong(pSize);
        Py_DECREF(pSize);
    }

    if ( ((size_t) (offset + byte_count)) > size) {
        PyErr_SetString(PyExc_ValueError, "Attempt to write past end of memory segment");
        goto error_return;
    }

    memcpy((self->address + offset), buffer, byte_count);

    Py_RETURN_NONE;

    error_return:
    return NULL;
}


static PyObject *
SysVSharedMemory_remove(SysVSharedMemory *self) {
    return shm_remove(self->id);
}


static PyObject *
shm_get_size(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_SIZE);
}

static PyObject *
shm_get_address(SysVSharedMemory *self) {
    return PyLong_FromVoidPtr(self->address);
}

static PyObject *
shm_get_attached(SysVSharedMemory *self) {
    if (self->address) 
        Py_RETURN_TRUE;
    else
        Py_RETURN_FALSE;
}

static PyObject *
shm_get_last_attach_time(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_ATTACH_TIME);
}

static PyObject *
shm_get_last_detach_time(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_DETACH_TIME);
}

static PyObject *
shm_get_last_change_time(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_CHANGE_TIME);
}

static PyObject *
shm_get_creator_pid(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_CREATOR_PID);
}

static PyObject *
shm_get_last_pid(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_LAST_AT_DT_PID);
}

static PyObject *
shm_get_number_attached(SysVSharedMemory *self) {
    return shm_get_value(self->id, SHM_NUMBER_ATTACHED);
}

static PyObject *
shm_get_key(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_KEY);
}

static PyObject *
shm_get_uid(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_UID);
}

static PyObject *
shm_get_cuid(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CUID);
}

static PyObject *
shm_get_cgid(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_CGID);
}

static PyObject *
shm_get_mode(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_MODE);
}

static int
shm_set_uid(SysVSharedMemory *self, PyObject *value) {
    uid_t new_uid;
    
    if (!PyInt_Check(value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'uid' must be an integer");
        goto error_return;
    }
    
    new_uid = (uid_t)PyInt_AsLong(value);
    
    if ((-1 == new_uid) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_UID, new_uid);

    error_return:
    return -1;
}


static PyObject *
shm_get_gid(SysVSharedMemory *self) {
    return shm_get_value(self->id, IPC_PERM_GID);
}

static int
shm_set_gid(SysVSharedMemory *self, PyObject *value) {
    gid_t new_gid;
    
    if (!PyInt_Check(value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'gid' must be an integer");
        goto error_return;
    }
    
    new_gid = (gid_t)PyInt_AsLong(value);
    
    if ((-1 == new_gid) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_GID, new_gid);

    error_return:
    return -1;
}

static int
shm_set_mode(SysVSharedMemory *self, PyObject *value) {
    unsigned short new_mode;
    
    if (!PyInt_Check(value)) {
		PyErr_Format(PyExc_TypeError, "attribute 'mode' must be an integer");
        goto error_return;
    }
    
    new_mode = (unsigned short)PyInt_AsLong(value);
    
    if (((unsigned short)-1 == new_mode) && PyErr_Occurred()) {
        // no idea what could have gone wrong -- punt it up to the caller
        goto error_return;
    }
    
    return shm_set_ipc_perm_value(self->id, IPC_PERM_MODE, new_mode);

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


static PyMemberDef SysVSemaphore_members[] = {
    {"id", T_INT, offsetof(SysVSemaphore, id), READONLY, "semaphore id"}, 
    {NULL} /* Sentinel */ 
};


static PyMethodDef SysVSemaphore_methods[] = { 
    {   "P", 
        (PyCFunction)SysVSemaphore_P, 
        METH_KEYWORDS, 
        "Acquire (decrement) the semaphore, waiting if necessary"
    },
    {   "acquire", 
        (PyCFunction)SysVSemaphore_acquire, 
        METH_KEYWORDS, 
        "Acquire (decrement) the semaphore, waiting if necessary"
    },
    {   "V",
        (PyCFunction)SysVSemaphore_V, 
        METH_KEYWORDS, 
        "Release (increment) the semaphore"
    }, 
    {   "release", 
        (PyCFunction)SysVSemaphore_release, 
        METH_KEYWORDS, 
        "Release (increment) the semaphore"
    },
    {   "Z",
        (PyCFunction)SysVSemaphore_Z, 
        METH_KEYWORDS, 
        "Waits until zee zemaphore is zero"
    }, 
    {   "remove",
        (PyCFunction)SysVSemaphore_remove, 
        METH_NOARGS, 
        "Removes (deletes) the semaphore from the system"
    }, 
    {NULL, NULL, 0, NULL} /* Sentinel */ 
};



static PyGetSetDef SysVSemaphore_gets_and_sets[] = { 
    {"key", (getter)sem_get_key, (setter)NULL, "key", NULL}, 
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




static PyTypeObject SysVSemaphoreType = {
    PyObject_HEAD_INIT(NULL)
    0, /*ob_size*/
    "sysv_ipc.SysVSemaphore", /*tp_name*/
    sizeof(SysVSemaphore), /*tp_basicsize*/
    0, /*tp_itemsize*/
    (destructor)SysVSemaphore_dealloc, /*tp_dealloc*/
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
    SysVSemaphore_methods, /* tp_methods */ 
    SysVSemaphore_members, /* tp_members */ 
    SysVSemaphore_gets_and_sets, /* tp_getset */ 
    0, /* tp_base */ 
    0, /* tp_dict */ 
    0, /* tp_descr_get */ 
    0, /* tp_descr_set */ 
    0, /* tp_dictoffset */ 
    (initproc)SysVSemaphore_init, /* tp_init */ 
    0, /* tp_alloc */ 
    SysVSemaphore_new, /* tp_new */ 
};



static PyMemberDef SysVSharedMemory_members[] = {
    {"id", T_INT, offsetof(SysVSharedMemory, id), READONLY, "semaphore id"}, 
    {NULL} /* Sentinel */ 
};


static PyMethodDef SysVSharedMemory_methods[] = { 
    {   "read",
        (PyCFunction)SysVSharedMemory_read, 
        METH_KEYWORDS, 
        "Read n bytes from the shared memory at the given offset into a Python string"
    },
    {   "write",
        (PyCFunction)SysVSharedMemory_write, 
        METH_KEYWORDS, 
        "Write the string to the shared memory at the offset given"
    }, 
    {   "remove",
        (PyCFunction)SysVSharedMemory_remove, 
        METH_NOARGS, 
        "Removes (deletes) the shared memory from the system"
    }, 
    {   "attach",
        (PyCFunction)SysVSharedMemory_attach, 
        METH_VARARGS, 
        "Attaches the shared memory"
    }, 
    {   "detach",
        (PyCFunction)SysVSharedMemory_detach, 
        METH_NOARGS, 
        "Detaches the shared memory"
    }, 
    {NULL, NULL, 0, NULL} /* Sentinel */ 
};



static PyGetSetDef SysVSharedMemory_gets_and_sets[] = { 
    {"size", (getter)shm_get_size, (setter)NULL, "size", NULL}, 
    {"address", (getter)shm_get_address, (setter)NULL, "address", NULL}, 
    {"attached", (getter)shm_get_attached, (setter)NULL, "attached", NULL}, 
    {"last_attach_time", (getter)shm_get_last_attach_time, (setter)NULL, "last_attach_time", NULL}, 
    {"last_detach_time", (getter)shm_get_last_detach_time, (setter)NULL, "last_detach_time", NULL}, 
    {"last_change_time", (getter)shm_get_last_change_time, (setter)NULL, "last_change_time", NULL}, 
    {"creator_pid", (getter)shm_get_creator_pid, (setter)NULL, "creator_pid", NULL}, 
    {"last_pid", (getter)shm_get_last_pid, (setter)NULL, "last_pid", NULL}, 
    {"number_attached", (getter)shm_get_number_attached, (setter)NULL, "number_attached", NULL}, 
    {"key", (getter)shm_get_key, (setter)NULL, "key", NULL}, 
    {"uid", (getter)shm_get_uid, (setter)shm_set_uid, "uid", NULL}, 
    {"gid", (getter)shm_get_gid, (setter)shm_set_gid, "gid", NULL}, 
    {"cuid", (getter)shm_get_cuid, (setter)NULL, "cuid", NULL}, 
    {"cgid", (getter)shm_get_cgid, (setter)NULL, "cgid", NULL}, 
    {"mode", (getter)shm_get_mode, (setter)shm_set_mode, "mode", NULL}, 
    {NULL} /* Sentinel */ 
};



static PyTypeObject SysVSharedMemoryType = {
    PyObject_HEAD_INIT(NULL)
    0, /*ob_size*/
    "sysv_ipc.SysVSharedMemory", /*tp_name*/
    sizeof(SysVSharedMemory), /*tp_basicsize*/
    0, /*tp_itemsize*/
    (destructor)SysVSharedMemory_dealloc, /*tp_dealloc*/
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
    SysVSharedMemory_methods, /* tp_methods */ 
    SysVSharedMemory_members, /* tp_members */ 
    SysVSharedMemory_gets_and_sets, /* tp_getset */ 
    0, /* tp_base */ 
    0, /* tp_dict */ 
    0, /* tp_descr_get */ 
    0, /* tp_descr_set */ 
    0, /* tp_dictoffset */ 
    (initproc)SysVSharedMemory_init, /* tp_init */ 
    0, /* tp_alloc */ 
    SysVSharedMemory_new, /* tp_new */ 
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

    if (PyType_Ready(&SysVSemaphoreType) < 0)
        goto error_return;
 
    if (PyType_Ready(&SysVSharedMemoryType) < 0)
        goto error_return;
 
    m = Py_InitModule3("sysv_ipc", module_methods, "System V IPC module");

    if (m == NULL) 
        goto error_return;

    PyModule_AddIntConstant(m, "PAGE_SIZE", my_getpagesize());
    // MAX_KEY = 2 to the power of however many bits are available in a key_t
    // which I calculate as sizeof(key_t) * 8 bits per byte, minus 1 because
    // the top bit might be a sign bit.
    PyModule_AddIntConstant(m, "MAX_KEY", pow(2, ((sizeof(key_t) * 8) - 1)));
#ifdef SEMVMX
    PyModule_AddIntConstant(m, "MAX_SEMAPHORE_VALUE", SEMVMX);
#endif
    PyModule_AddIntConstant(m, "IPC_CREAT", IPC_CREAT);
    PyModule_AddIntConstant(m, "IPC_EXCL", IPC_EXCL);
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

    Py_INCREF(&SysVSemaphoreType);
    PyModule_AddObject(m, "SysVSemaphore", (PyObject *)&SysVSemaphoreType);

    Py_INCREF(&SysVSharedMemoryType);
    PyModule_AddObject(m, "SysVSharedMemory", (PyObject *)&SysVSharedMemoryType);
    
    
    if (!(pSysVIpcException = PyErr_NewException("sysv_ipc.SystemVIpcError", NULL, NULL)))
        goto error_return;
    
    if (!(pSysVIpcInternalException = PyErr_NewException("sysv_ipc.SystemVIpcInternalError", NULL, NULL)))
        goto error_return;
    
    if (!(pSysVIpcPermissionsException = PyErr_NewException("sysv_ipc.SystemVIpcPermissionsError", pSysVIpcException, NULL)))
        goto error_return;
    
    if (!(pSysVIpcExistentialException = PyErr_NewException("sysv_ipc.SystemVIpcExistentialError", pSysVIpcException, NULL)))
        goto error_return;
    
    if (!(pSysVIpcBusyException = PyErr_NewException("sysv_ipc.SystemVIpcBusyError", pSysVIpcException, NULL)))
        goto error_return;
    
    // if (!(pSysVIpcTimeoutException = PyErr_NewException("sysv_ipc.SystemVIpcTimeoutError", pSysVIpcException, NULL)))
    //     goto error_return;
    // 
    if (!(pSysVIpcNotAttachedException = PyErr_NewException("sysv_ipc.SystemVIpcNotAttachedError", pSysVIpcException, NULL)))
        goto error_return;
    
    

    return;

    error_return:
    return;
}




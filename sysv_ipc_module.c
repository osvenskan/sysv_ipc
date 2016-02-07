/*
    sysv_ipc - A Python module for accessing System V semaphores
                and shared memory.

    Copyright (C) 2008 - 2009 Philip Semanchuk

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
#include "Python.h"
#include "structmember.h"

// for memset
#include <string.h>

// For the math surrounding timeouts for semtimedop()
#include <math.h>

#include "common.h"
#include "semaphore.h"
#include "memory.h"
#include "mq.h"

PyObject *pBaseException;
PyObject *pInternalException;
PyObject *pPermissionsException;
PyObject *pExistentialException;
PyObject *pBusyException;
PyObject *pNotAttachedException;

/* 

        Module methods 

*/

static PyObject *
sysv_ipc_remove_semaphore(PyObject *self, PyObject *args) {
    int id; 

    if (!PyArg_ParseTuple(args, "i", &id))
        goto error_return;

    DPRINTF("removing sem with id %d\n", id);
    if (NULL == sem_remove(id))
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


static PyObject *
sysv_ipc_remove_message_queue(PyObject *self, PyObject *args) {
    int id; 

    if (!PyArg_ParseTuple(args, "i", &id))
        goto error_return;

    return mq_remove(id);
    
    error_return:
    return NULL;    
}


static PyMemberDef Semaphore_members[] = {
    {"id", T_INT, offsetof(Semaphore, id), READONLY, "The id assigned by the system"}, 
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
    {   "key", 
        (getter)sem_get_key, 
        (setter)NULL, 
        "The key passed to the constructor", 
        NULL
    }, 
    {   "value", 
        (getter)sem_get_value, 
        (setter)sem_set_value, 
        "The semaphore's current value", 
        NULL
    },
    {   "undo",
        (getter)sem_get_undo,
        (setter)sem_set_undo,
        "When True, acquire/release operations will be undone when the process exits. Non-portable.",
        NULL
    },
    {   "block",
        (getter)sem_get_block,
        (setter)sem_set_block,
        "When True (the default), calls to acquire/release/P/V/Z will wait (block) if the semaphore is busy",
        NULL
    },
    {   "mode",
        (getter)sem_get_mode,
        (setter)sem_set_mode,
        "Permissions",
        NULL
    }, 
    {   "uid", 
        (getter)sem_get_uid, 
        (setter)sem_set_uid, 
        "The semaphore's UID", 
        NULL
    }, 
    {   "gid",
        (getter)sem_get_gid,
        (setter)sem_set_gid,
        "The semaphore's GID",
        NULL
    }, 
    {   "cuid",
        (getter)sem_get_c_uid,
        (setter)NULL,
        "The semaphore creator's UID. Read only.",
        NULL
    }, 
    {   "cgid",
        (getter)sem_get_c_gid,
        (setter)NULL,
        "The semaphore creator's GID. Read only.",
        NULL
    }, 
    {   "last_pid", 
        (getter)sem_get_last_pid, 
        (setter)NULL, 
        "The id of the last process to call acquire()/release()/Z() on this semaphore. Read only.",
        NULL
    }, 
    {   "waiting_for_nonzero",
        (getter)sem_get_waiting_for_nonzero,
        (setter)NULL,
        "The number of processes waiting for the semaphore to become non-zero. Read only.",
        NULL
    },
    {   "waiting_for_zero", 
        (getter)sem_get_waiting_for_zero, 
        (setter)NULL, 
        "The number of processes waiting for the semaphore to become zero. Read only.",
        NULL
    },
    {   "o_time", 
        (getter)sem_get_o_time, 
        (setter)NULL, 
        "The last time semop (acquire/release/P/V/Z) was called on this semaphore. Initialized to zero. Read only.", 
        NULL
    },
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
    {"id", T_INT, offsetof(SharedMemory, id), READONLY, "The id assigned by the system"},
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
    {   "key",
        (getter)shm_get_key,
        (setter)NULL,
        "The key passed to the constructor. Read only.",
        NULL
    },
    {   "size",
        (getter)shm_get_size,
        (setter)NULL,
        "The size of the segment in bytes. Read only.",
        NULL
    }, 
    {   "address",
        (getter)shm_get_address,
        (setter)NULL,
        "The memory address of the segment. Read only.",
        NULL
    },
    {   "attached",
        (getter)shm_get_attached,
        (setter)NULL,
        "True if the segment is attached. Read only.",
        NULL
    },
    {   "last_attach_time",
        (getter)shm_get_last_attach_time,
        (setter)NULL,
        "The most recent time this segment was attached. Read only.",
        NULL
    },
    {   "last_detach_time",
        (getter)shm_get_last_detach_time,
        (setter)NULL,
        "The most recent time this segment was detached. Read only.",
        NULL
    },
    {   "last_change_time", 
        (getter)shm_get_last_change_time, 
        (setter)NULL, 
        "The time of the most recent change to this segment's uid, gid, mode, or the time the segment was removed. Read only.", 
        NULL
    }, 
    {   "creator_pid",
        (getter)shm_get_creator_pid,
        (setter)NULL,
        "The process id of the creator. Read only.",
        NULL
    },
    {   "last_pid", 
        (getter)shm_get_last_pid, 
        (setter)NULL, 
        "The id of the process that performed the most recent attach or detach. Read only.", 
        NULL
    }, 
    {   "number_attached", 
        (getter)shm_get_number_attached, 
        (setter)NULL, 
        "The current number of attached processes. Read only.", 
        NULL
    }, 
    {   "uid", 
        (getter)shm_get_uid, 
        (setter)shm_set_uid, 
        "The segment's UID.", 
        NULL
    }, 
    {   "gid", 
        (getter)shm_get_gid, 
        (setter)shm_set_gid, 
        "The segment's GID.", 
        NULL
    }, 
    {   "cuid",     
        (getter)shm_get_cuid, 
        (setter)NULL, 
        "The UID of the segment's creator. Read only.", 
        NULL
    }, 
    {   "cgid",     
        (getter)shm_get_cgid, 
        (setter)NULL, 
        "The GID of the segment's creator. Read only.", 
        NULL
    }, 
    {   "mode", 
        (getter)shm_get_mode, 
        (setter)shm_set_mode, 
        "Permissions.", 
        NULL
    }, 
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




static PyMemberDef MessageQueue_members[] = {
    {"id", T_INT, offsetof(MessageQueue, id), READONLY, "Message queue id"}, 
    {NULL} /* Sentinel */ 
};


static PyMethodDef MessageQueue_methods[] = { 
    {   "send",
        (PyCFunction)MessageQueue_send,
        METH_KEYWORDS, 
        "Place a message on the queue"
    },
    {   "receive",
        (PyCFunction)MessageQueue_receive,
        METH_KEYWORDS, 
        "Receive a message from the queue"
    },
    {   "remove",
        (PyCFunction)MessageQueue_remove, 
        METH_NOARGS, 
        "Removes (deletes) the queue from the system"
    }, 
    {NULL, NULL, 0, NULL} /* Sentinel */ 
};



static PyGetSetDef MessageQueue_gets_and_sets[] = { 
    {   "key", 
        (getter)mq_get_key,
        (setter)NULL, 
        "The key passed to the constructor.", 
        NULL
    },
    {   "last_send_time", 
        (getter)mq_get_last_send_time, 
        (setter)NULL, 
        "A Unix timestamp representing the last time a message was sent.", 
        NULL
    }, 
    {   "last_receive_time", 
        (getter)mq_get_last_receive_time, 
        (setter)NULL, 
        "A Unix timestamp representing the last time a message was received.", 
        NULL
    }, 
    {   "last_change_time", 
        (getter)mq_get_last_change_time, 
        (setter)NULL, 
        "A Unix timestamp representing the last time the queue was changed.",
        NULL
    }, 
    {   "current_messages", 
        (getter)mq_get_current_messages, 
        (setter)NULL, 
        "The number of messages currently in the queue", 
        NULL
    },
    {   "last_send_pid", 
        (getter)mq_get_last_send_pid, 
        (setter)NULL, 
        "The id of the last process which sent via the queue", 
        NULL}, 
    {   "last_receive_pid", 
        (getter)mq_get_last_receive_pid, 
        (setter)NULL, 
        "The id of the last process which received from the queue", 
        NULL
    }, 
    {   "max_size", 
        (getter)mq_get_max_size, 
        (setter)mq_set_max_size, 
        "The maximum size of the queue (in bytes). Read-write if you have sufficient privileges.", 
        NULL
    }, 
    {   "mode", 
        (getter)mq_get_mode, 
        (setter)mq_set_mode, 
        "Permissions", 
        NULL
    }, 
    {   "uid", 
        (getter)mq_get_uid, 
        (setter)mq_set_uid, 
        "The queue's UID.", 
        NULL
    }, 
    {   "gid", 
        (getter)mq_get_gid, 
        (setter)mq_set_gid, 
        "The queue's GID.", 
        NULL
    }, 
    {   "cuid",     
        (getter)mq_get_c_uid, 
        (setter)NULL, 
        "The UID of the queue's creator. Read only.", 
        NULL
    }, 
    {   "cgid",     
        (getter)mq_get_c_gid, 
        (setter)NULL, 
        "The GID of the queue's creator. Read only.", 
        NULL
    },
    {NULL} /* Sentinel */ 
};


static PyTypeObject MessageQueueType = {
    PyObject_HEAD_INIT(NULL)
    0, /*ob_size*/
    "sysv_ipc.MessageQueue", /*tp_name*/
    sizeof(MessageQueue), /*tp_basicsize*/
    0, /*tp_itemsize*/
    (destructor)MessageQueue_dealloc, /*tp_dealloc*/
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
    "System V message queue objects", /* tp_doc */
    0, /* tp_traverse */ 
    0, /* tp_clear */ 
    0, /* tp_richcompare */ 
    0, /* tp_weaklistoffset */ 
    0, /* tp_iter */ 
    0, /* tp_iternext */ 
    MessageQueue_methods, /* tp_methods */ 
    MessageQueue_members, /* tp_members */ 
    MessageQueue_gets_and_sets, /* tp_getset */ 
    0, /* tp_base */ 
    0, /* tp_dict */ 
    0, /* tp_descr_get */ 
    0, /* tp_descr_set */ 
    0, /* tp_dictoffset */ 
    (initproc)MessageQueue_init, /* tp_init */ 
    0, /* tp_alloc */ 
    MessageQueue_new, /* tp_new */ 
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
        "Remove shared memory identified by id"
    },
    {   "remove_message_queue",
        (PyCFunction)sysv_ipc_remove_message_queue,
        METH_VARARGS,
        "Remove the message queue identified by id"
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

    if (PyType_Ready(&MessageQueueType) < 0)
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
    
    Py_INCREF(&MessageQueueType);
    PyModule_AddObject(m, "MessageQueue", (PyObject *)&MessageQueueType);
    
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


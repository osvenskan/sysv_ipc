#include "Python.h"
#include "structmember.h"

#if PY_MAJOR_VERSION >= 0x02050000
#define PY_SSIZE_T_CLEAN
#define PY_STRING_LENGTH_MAX  PY_SSIZE_T
#else
#define PY_STRING_LENGTH_MAX  INT_MAX
#endif

/* In sys/ipc.h under OS X, I get stuck with the old, "do not use" version
of the ipc_perm struct. This is unavoidable because Python.h #undefs 
_POSIX_C_SOURCE and _XOPEN_SOURCE. This causes cdefs.h to #define 
__DARWIN_UNIX03 to 0 (implying the "default" compilation environment, as 
opposed to "strict") and therefore I get the crappy, old version of the
struct. I don't think there's any way around this unless Python changes
its headers.
*/ 
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include "probe_results.h"

/* Struct to contain a key which can be None */
typedef struct {
    int is_none;
    key_t value;
} NoneableKey;


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
    SHM_NUMBER_ATTACHED,
    MQ_LAST_SEND_TIME,
    MQ_LAST_RECEIVE_TIME,
    MQ_LAST_CHANGE_TIME,
    MQ_CURRENT_MESSAGES,
    MQ_QUEUE_BYTES_MAX,
    MQ_LAST_SEND_PID,
    MQ_LAST_RECEIVE_PID    
};

// Shorthand for lazy typists like me
#define IPC_CREX  (IPC_CREAT | IPC_EXCL)

#ifdef SYSV_IPC_DEBUG
#define DPRINTF(fmt, args...) fprintf(stderr, "+++ " fmt, ## args)
#else
#define DPRINTF(fmt, args...)
#endif


// key_t is not guaranteed to be an arithmetic type, but that guarantee
// is proposed for standards track and is (I hope!) true in practice.
// ref: http://www.opengroup.org/austin/mailarchives/ag/msg09208.html
// ref: http://www.opengroup.org/austin/mailarchives/ag-review/msg01783.html
#define KEY_T_TO_PY(key)   PyInt_FromLong(key)

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

// The SUS guarantees a msglen_t to be an unsigned integer type. Ditto: msgqnum_t.
// ref: http://www.opengroup.org/onlinepubs/000095399/basedefs/sys/msg.h.html
#define MSGLEN_T_TO_PY(msglen)   py_int_or_long_from_ulong(msglen)
#define MSGQNUM_T_TO_PY(msgqnum)   py_int_or_long_from_ulong(msgqnum)

/* Utility functions */
key_t get_random_key(void);
PyObject *py_int_or_long_from_ulong(unsigned long);
int convert_key_param(PyObject *, void *);

/* Custom Exceptions/Errors */
extern PyObject *pBaseException;
extern PyObject *pInternalException;
extern PyObject *pPermissionsException;
extern PyObject *pExistentialException;
extern PyObject *pBusyException;
extern PyObject *pNotAttachedException;

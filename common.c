#include "Python.h"
#include "structmember.h"

#include "common.h"

#include <stdlib.h>

key_t 
get_random_key(void) {
    int key;
    
    // I think IPC_PRIVATE is always #defined as 0, but in case it isn't...
    do {
        // ref: http://www.c-faq.com/lib/randrange.html
        key = ((int)((double)rand() / ((double)RAND_MAX + 1) * KEY_MAX)) + 1;
    } while (key == IPC_PRIVATE);

    return (key_t)key;
}

PyObject *
py_int_or_long_from_ulong(unsigned long value) {
    // Python ints are guaranteed to accept up to LONG_MAX. Anything 
    // larger needs to be a Python long.
    if (value > LONG_MAX)
        return PyLong_FromUnsignedLong(value);
    else
        return PyInt_FromLong(value);
}


int
convert_key_param(PyObject *py_key, void *converted_key) {
    // Converts a PyObject into a key if possible. Returns 0 on failure.
    // The converted_key param should point to a NoneableKey type.
    // None is an acceptable key, in which case converted_key->is_none 
    // is non-zero and converted_key->value is undefined.
    int rc = 0;
    long key = 0;

    ((NoneableKey *)converted_key)->is_none = 0;
    
    if (py_key == Py_None) {
        rc = 1;
        ((NoneableKey *)converted_key)->is_none = 1;
    }
    else if (PyInt_Check(py_key)) {
        rc = 1;
        key = PyInt_AsLong(py_key);
    }
    else if (PyLong_Check(py_key)) {
        rc = 1;
        key = PyLong_AsLong(py_key);
        if (PyErr_Occurred()) {
            // This happens when the Python long is too big for a C long.
            rc = 0;
            PyErr_Format(PyExc_ValueError, 
                         "Key must be between 0 and %ld (KEY_MAX)", 
                         (long)KEY_MAX);
        }
    }
    
    if (rc) {
        // Param is OK
        if (! ((NoneableKey *)converted_key)->is_none) {
            // It's not None; ensure it is in range
            if ((key >= 0) && (key <= KEY_MAX))
                ((NoneableKey *)converted_key)->value = (key_t)key;
            else {
                rc = 0;
                PyErr_Format(PyExc_ValueError, 
                             "Key must be between 0 and %ld (KEY_MAX)", 
                             (long)KEY_MAX);
            }
        }
    }
    else
        PyErr_SetString(PyExc_TypeError, "Key must be an integer or None");

    return rc;
}



// 
// int 
// convert_key_param(PyObject *py_key, void *converted_key) {
//     // Converts a PyObject into a key if possible. Returns 0 on failure.
//     // The converted_key param should point to a key_t type.
//     int rc = 0;
//     long key;
//     
//     // Convert from the Python type to a C long
//     if (PyInt_Check(py_key)) {
//         rc = 1;
//         key = PyInt_AsLong(py_key);
//     }
//     else if (PyLong_Check(py_key)) {
//         rc = 1;
//         key = PyLong_AsLong(py_key);
//         if (PyErr_Occurred()) {
//             // This happens when the Python long is too big for a C long.
//             rc = 0;
//             PyErr_Format(PyExc_ValueError, 
//                          "Key must be between 0 and %ld (KEY_MAX)", 
//                          (long)KEY_MAX);
//         }
//     }
//     
//     if (rc) {
//         // Ensure it is in range
//         if ((key >= 0) && (key <= KEY_MAX))
//             *((key_t *)converted_key) = (key_t)key;
//         else {
//             rc = 0;
//             PyErr_Format(PyExc_ValueError, 
//                          "Key must be between 0 and %ld (KEY_MAX)", 
//                          (long)KEY_MAX);
//         }
//     }
//     else
//         PyErr_SetString(PyExc_TypeError, "Key must be an integer");
// 
//     return rc;
// }
// 



#include "Python.h"
#include "structmember.h"

#include "common.h"

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
    // The converted_key param should point to a key_t type.
    int rc = 0;
    long key;
    
    // Convert from the Python type to a C long
    if (PyInt_Check(py_key)) {
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
        // Ensure it is in range
        if ((key >= 0) && (key <= KEY_MAX))
            *((key_t *)converted_key) = (key_t)key;
        else {
            rc = 0;
            PyErr_Format(PyExc_ValueError, 
                         "Key must be between 0 and %ld (KEY_MAX)", 
                         (long)KEY_MAX);
        }
    }
    else
        PyErr_SetString(PyExc_TypeError, "Key must be an integer");

    return rc;
}


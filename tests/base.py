# Python imports
import unittest
import random
import time

# Project imports
import sysv_ipc


def make_key():
    """Generate a random key suitable for an IPC object."""
    return random.randint(sysv_ipc.KEY_MIN, sysv_ipc.KEY_MAX)


def sleep_past_granularity():
    """A utility method that encapsulates a type-specific detail of testing.

    I test all of the time-related variables in the IPC structs (o_time, shm_atime, shm_dtime,
    shm_ctime, msg_ctime, msg_stime, and msg_rtime) to ensure they change when they're supposed
    to (e.g. when a segment is detached, for shm_dtime). For variables that are initialized to 0
    (like o_time), it's easy to verify that they're 0 to start with and then non-zero after the
    change.

    Other variables (like shm_ctime) are trickier to test because they're already non-zero
    immediately after the object is created. My test has to save the value, do something that
    should change it, and then compare the saved value to the current one via assertNotEqual().

    Some (most? all?) systems define those time-related values as integral values (int or long),
    so their granularity is only 1 second. If I don't force at least 1 second to elapse between
    the statement where I save the value and the statement that should change it, they'll almost
    always happen in the same second and the assertNotEqual() even though all code (mine and the
    system) has behaved correctly.

    This method sleeps for 1.1 seconds to avoid the problem described above.
    """
    time.sleep(1.1)


class Base(unittest.TestCase):
    """Base class for test cases."""
    def assertWriteToReadOnlyPropertyFails(self, target_object, property_name, value):
        """test that writing to a readonly property raises an exception with the expected msg"""
        with self.assertRaises(AttributeError) as context:
            setattr(target_object, property_name, value)

        # In addition to checking that AttributeError is raised, I also check the message text.
        actual = str(context.exception)

        if property_name == 'id':
            # For some reason 'id' gets a different message.
            expected = 'readonly attribute'
        else:
            # Extract the class name. str() returns something like this --
            #    <class 'sysv_ipc.SharedMemory'>
            # From that, I only want this bit --
            #    sysv_ipc.SharedMemory
            class_name = str(target_object.__class__)[8:-2]

            # Under CPython, the class name is prefixed with the module name. Under PyPy, it is
            # not. I remove the module name if present.
            if class_name.startswith('sysv_ipc.'):
                class_name = class_name[9:]

            expected = f"attribute '{property_name}' of '{class_name}' objects is not writable"

        assert (actual == expected), f'actual: `{actual}`, expected: `{expected}`'

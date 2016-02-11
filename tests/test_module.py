# Python imports
# Don't add any from __future__ imports here. This code should execute
# against standard Python.
import unittest
import os
import resource
import warnings

# Project imports
import sysv_ipc
# Hack -- add tests directory to sys.path so Python 3 can find base.py.
import sys
sys.path.insert(0, os.path.join(os.getcwd(), 'tests'))  # noqa - tell flake8 to chill
import base as tests_base

ONE_MILLION = 1000000


class TestModuleConstants(tests_base.Base):
    """Check that the sysv_ipc module-level constants are defined as expected"""
    def test_constant_values(self):
        """test that constants are what I expect"""
        self.assertEqual(sysv_ipc.IPC_CREX, sysv_ipc.IPC_CREAT | sysv_ipc.IPC_EXCL)
        self.assertEqual(sysv_ipc.PAGE_SIZE, resource.getpagesize())

        self.assertIn(sysv_ipc.SEMAPHORE_TIMEOUT_SUPPORTED, (True, False))

        self.assertGreaterEqual(sysv_ipc.SEMAPHORE_VALUE_MAX, 1)

        self.assertTrue(isinstance(sysv_ipc.VERSION, str))


class TestModuleErrors(tests_base.Base):
    """Exercise the exceptions defined by the module"""
    def test_errors(self):
        self.assertTrue(issubclass(sysv_ipc.Error, Exception))
        self.assertTrue(issubclass(sysv_ipc.InternalError, sysv_ipc.Error))
        self.assertTrue(issubclass(sysv_ipc.PermissionsError, sysv_ipc.Error))
        self.assertTrue(issubclass(sysv_ipc.ExistentialError, sysv_ipc.Error))
        self.assertTrue(issubclass(sysv_ipc.BusyError, sysv_ipc.Error))
        self.assertTrue(issubclass(sysv_ipc.NotAttachedError, sysv_ipc.Error))


class TestModuleFunctions(tests_base.Base):
    """Exercise the sysv_ipc module-level functions"""
    def test_attach(self):
        """Exercise attach()"""
        # Create memory, write something to it, then detach
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX)
        mem.write('hello world')
        mem.detach()
        self.assertFalse(mem.attached)
        self.assertEqual(mem.number_attached, 0)

        # Reattach memory via a different SharedMemory instance
        mem2 = sysv_ipc.attach(mem.id)
        self.assertFalse(mem.attached)
        self.assertTrue(mem2.attached)
        self.assertEqual(mem.number_attached, 1)
        self.assertEqual(mem2.number_attached, 1)

        self.assertEqual(mem2.read(len('hello world')), b'hello world')

        mem2.detach()

        mem.remove()

        self.assertRaises(sysv_ipc.ExistentialError, sysv_ipc.SharedMemory, mem.key)

    def test_attach_kwargs(self):
        """Ensure attach takes kwargs as advertised"""
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX)
        mem.write('hello world')
        mem.detach()
        mem2 = sysv_ipc.attach(mem.id, flags=0)
        mem2.detach()
        mem.remove()

    def test_ftok(self):
        """Exercise ftok()"""
        with warnings.catch_warnings(record=True) as recorded_warnings:
            warnings.simplefilter("always")

            sysv_ipc.ftok('.', 42)

            self.assertEqual(len(recorded_warnings), 1)
            self.assertTrue(issubclass(recorded_warnings[-1].category, Warning))

        with warnings.catch_warnings(record=True) as recorded_warnings:
            warnings.simplefilter("always")

            sysv_ipc.ftok('.', 42, silence_warning=True)

            self.assertEqual(len(recorded_warnings), 0)

    def test_ftok_kwargs(self):
        """Ensure ftok() takes kwargs as advertised"""
        sysv_ipc.ftok('.', 42, silence_warning=True)

    def test_remove_semaphore(self):
        """Exercise remove_semaphore()"""
        sem = sysv_ipc.Semaphore(None, sysv_ipc.IPC_CREX)

        sysv_ipc.remove_semaphore(sem.id)

        with self.assertRaises(sysv_ipc.ExistentialError):
            sysv_ipc.Semaphore(sem.key)

    def test_remove_shared_memory(self):
        """Exercise remove_shared_memory()"""
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX)

        sysv_ipc.remove_shared_memory(mem.id)

        with self.assertRaises(sysv_ipc.ExistentialError):
            sysv_ipc.SharedMemory(mem.key)

    def test_remove_message_queue(self):
        """Exercise remove_message_queue()"""
        mq = sysv_ipc.MessageQueue(None, sysv_ipc.IPC_CREX)

        sysv_ipc.remove_message_queue(mq.id)

        with self.assertRaises(sysv_ipc.ExistentialError):
            sysv_ipc.MessageQueue(mq.key)


if __name__ == '__main__':
    unittest.main()

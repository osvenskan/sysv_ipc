# Python imports
# Don't add any from __future__ imports here. This code should execute
# against standard Python.
import unittest
import time
import os

# Project imports
# Hack -- add tests directory to sys.path so Python 3 can find base.py.
import sys
sys.path.insert(0, os.path.join(os.getcwd(), 'tests'))  # noqa - tell flake8 to chill
import base as tests_base
import sysv_ipc

# Not tested --
# - mode seems to be settable and readable, but ignored by the OS
# - address param of attach()
# - attempt to write to segment attached with SHM_RDONLY gives a segfault under OS X and Linux.


class SharedMemoryTestBase(tests_base.Base):
    """base class for SharedMemory test classes"""
    # SIZE should be something that's not a power of 2 since that's more
    # likely to expose odd behavior.
    SIZE = 3333

    def setUp(self):
        self.mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, size=self.SIZE)

    def tearDown(self):
        if self.mem:
            if self.mem.attached:
                self.mem.detach()
            self.mem.remove()

    def assertWriteToReadOnlyPropertyFails(self, property_name, value):
        """test that writing to a readonly property raises TypeError"""
        tests_base.Base.assertWriteToReadOnlyPropertyFails(self, self.mem,
                                                           property_name, value)


class TestSharedMemoryCreation(SharedMemoryTestBase):
    """Exercise stuff related to creating SharedMemory"""

    def get_block_size(self):
        """Return block size as reported by operating system"""
        return os.statvfs('.')[1]

    def test_no_flags(self):
        """tests that opening a SharedMemory with no flags opens the existing
        SharedMemory and doesn't create a new SharedMemory"""
        mem_copy = sysv_ipc.SharedMemory(self.mem.key)
        self.assertEqual(self.mem.key, mem_copy.key)

    def test_IPC_CREAT_existing(self):
        """tests sysv_ipc.IPC_CREAT to open an existing SharedMemory without IPC_EXCL"""
        mem_copy = sysv_ipc.SharedMemory(self.mem.key, sysv_ipc.IPC_CREAT)

        self.assertEqual(self.mem.key, mem_copy.key)

    def test_IPC_CREAT_new(self):
        """tests sysv_ipc.IPC_CREAT to create a new SharedMemory without IPC_EXCL"""
        # I can't pass None for the name unless I also pass IPC_EXCL.
        key = tests_base.make_key()

        # Note: this method of finding an unused key is vulnerable to a race
        # condition. It's good enough for test, but don't copy it for use in
        # production code!
        key_is_available = False
        while not key_is_available:
            try:
                mem = sysv_ipc.SharedMemory(key)
                mem.detach()
                mem.remove()
            except sysv_ipc.ExistentialError:
                key_is_available = True
            else:
                key = tests_base.make_key()

        mem = sysv_ipc.SharedMemory(key, sysv_ipc.IPC_CREAT, size=sysv_ipc.PAGE_SIZE)

        self.assertIsNotNone(mem)

        mem.detach()
        mem.remove()

    def test_IPC_EXCL(self):
        """tests IPC_CREAT | IPC_EXCL prevents opening an existing SharedMemory"""
        with self.assertRaises(sysv_ipc.ExistentialError):
            # I have to specify the size, otherwise I get ValueError: The size is invalid
            # which trumps the ExistentialError I'm looking for.
            sysv_ipc.SharedMemory(self.mem.key, sysv_ipc.IPC_CREX, size=self.mem.size)

    def test_randomly_generated_key(self):
        """tests that the randomly-generated key works"""
        # This is tested implicitly elsewhere but I want to test it explicitly
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX)
        self.assertIsNotNone(mem.key)
        self.assertGreaterEqual(mem.key, sysv_ipc.KEY_MIN)
        self.assertLessEqual(mem.key, sysv_ipc.KEY_MAX)
        mem.detach()
        mem.remove()

    # don't bother testing mode, it's ignored by the OS?

    def test_default_flags(self):
        """tests that the flag is 0 by default (==> open existing)"""
        mem = sysv_ipc.SharedMemory(self.mem.key)
        self.assertEqual(self.mem.id, mem.id)
        mem.detach()

    def test_size(self):
        """test that the size specified is (somewhat) respected"""
        # My experience with posix_ipc was that the Linuxes I tested respect the exact size
        # specified in the SharedMemory() ctor. # e.g. when self.SIZE = 3333, the mmapped file is
        # also 3333 bytes.
        #
        # OS X's mmapped files always have sizes that are mod 4096 which is probably block size.
        #
        # I haven't tested other operating systems, and I haven't run this experiment with sysv_ipc.
        #
        # AFAICT the specification doesn't demand that the size has to match
        # exactly, so this code accepts either value as correct.
        block_size = self.get_block_size()

        delta = self.SIZE % block_size

        if delta:
            # Round up to nearest block size
            crude_size = (self.SIZE - delta) + block_size
        else:
            crude_size = self.SIZE

        self.assertIn(self.mem.size, (self.SIZE, crude_size))

    def test_default_init_character(self):
        """tests that the init_character defaulted to a blank"""
        self.assertEqual(self.mem.read(self.mem.size), b' ' * self.mem.size)

    def test_nondefault_init_character(self):
        """tests that the init_character can be something other than the default"""
        init_character = b'@'
        mem = sysv_ipc.SharedMemory(None, sysv_ipc.IPC_CREX, init_character=init_character)
        self.assertEqual(mem.read(mem.size), init_character * mem.size)
        mem.detach()
        mem.remove()

    def test_autoattach(self):
        """tests that attach() is performed as part of init"""
        self.assertTrue(self.mem.attached)

    def test_kwargs(self):
        """ensure init accepts keyword args as advertised"""
        # mode 0x180 = 0600. Octal is difficult to express in Python 2/3 compatible code.
        mem = sysv_ipc.SharedMemory(None, flags=sysv_ipc.IPC_CREX, mode=0x180,
                                    size=sysv_ipc.PAGE_SIZE, init_character=b'x')
        mem.detach()
        mem.remove()


class TestSharedMemoryAttachDetach(SharedMemoryTestBase):
    """Exercise attach() and detach()"""
    def test_detach(self):
        """exercise detach()"""
        self.assertTrue(self.mem.attached)
        self.mem.detach()
        self.assertFalse(self.mem.attached)
        with self.assertRaises(sysv_ipc.NotAttachedError):
            self.mem.read(1)

    def test_attach(self):
        """exercise attach()"""
        self.assertTrue(self.mem.attached)
        self.mem.detach()
        self.assertFalse(self.mem.attached)
        self.mem.attach()
        # Should not raise an error
        self.mem.read(1)

    def test_attach_read_only(self):
        """exercise attach(SHM_RDONLY)"""
        self.assertTrue(self.mem.attached)
        self.mem.detach()
        self.assertFalse(self.mem.attached)
        self.mem.attach(None, sysv_ipc.SHM_RDONLY)
        with self.assertRaises(OSError):
            self.mem.write(' ')

    def test_attach_kwargs(self):
        """ensure attach() takes kwargs as advertised"""
        self.mem.detach()
        self.mem.attach(address=None, flags=0)


class TestSharedMemoryReadWrite(SharedMemoryTestBase):
    """Exercise read() and write()"""
    def test_simple_read_write(self):
        test_string = b'abcdefg'

        self.assertEqual(self.mem.read(20), b' ' * 20)
        self.mem.write(test_string)
        self.assertEqual(self.mem.read(len(test_string)), test_string)

    def test_read_no_byte_count(self):
        """test the default return-all aspect of read()"""
        self.assertEqual(self.mem.read(), b' ' * self.mem.size)

    def test_read_byte_count(self):
        """test the byte_count param of read()"""
        self.assertEqual(len(self.mem.read(5)), 5)

    def test_read_offset(self):
        """test the offset param of read()"""
        test_string = 'abcdefg'
        self.mem.write(test_string)
        s = self.mem.read(5, 2)
        self.assertEqual(s, b'cdefg')

    def test_read_keywords(self):
        """ensure read() accepts kwargs"""
        test_string = 'abcdefg'
        self.mem.write(test_string)
        s = self.mem.read(byte_count=5, offset=2)
        self.assertEqual(s, b'cdefg')

    def test_read_past_end_of_segment(self):
        """ensure I don't crash if I try to read past the end of the segment"""
        self.assertEqual(self.mem.read(self.mem.size + 100), b' ' * self.mem.size)

    def test_read_past_end_of_segment_with_offset(self):
        """ensure I don't crash if I try to read past the end of the segment using an offset"""
        self.assertEqual(self.mem.read(self.mem.size + 100, 100), b' ' * (self.mem.size - 100))

    def test_read_bad_offset(self):
        """Ensure ValueError is raised when I use a bad offset"""
        with self.assertRaises(ValueError):
            self.mem.read(offset=self.mem.size)

        with self.assertRaises(ValueError):
            self.mem.read(offset=self.mem.size + 1)

        with self.assertRaises(ValueError):
            self.mem.read(offset=-1)

    def test_write_offset(self):
        """test the offset param of write()"""
        test_string = 'abcdefg'
        self.mem.write(test_string, 3)
        self.assertEqual(self.mem.read(10), b'   abcdefg')

    def test_write_keyword(self):
        """ensure write() accepts a keyword arg"""
        test_string = 'abcdefg'
        self.mem.write(test_string, offset=3)
        self.assertEqual(self.mem.read(10), b'   abcdefg')

    def test_write_past_end_of_segment(self):
        """ensure ValueError is raised if I try to write past the end of the segment"""
        with self.assertRaises(ValueError):
            self.mem.write('x' * (self.mem.size + 100))

    def test_write_past_end_of_segment_with_offset(self):
        """ensure ValueError is raised if I try to write past the end of the segment w/an offset"""
        with self.assertRaises(ValueError):
            self.mem.write('x' * (self.mem.size - 50), 100)

    def test_write_bad_offset(self):
        """ensure ValueError is raised if I try to write using a bad offset"""
        with self.assertRaises(ValueError):
            self.mem.write('x', -1)

    def test_ascii_null(self):
        """ensure I can write & read 0x00"""
        test_string = b'abc' + bytes(0) + b'def'
        self.mem.write(test_string)
        self.assertEqual(self.mem.read(len(test_string)), test_string)

    def test_utf8(self):
        """Test writing encoded Unicode"""
        test_string = 'G' + '\u00F6' + 'teborg'
        test_string = test_string.encode('utf-8')
        self.mem.write(test_string)
        self.assertEqual(self.mem.read(len(test_string)), test_string)

    def test_read_kwargs(self):
        """ensure read() accepts keyword args as advertised"""
        self.mem.read(byte_count=1, offset=0)

    def test_write_kwargs(self):
        """ensure write() accepts keyword args as advertised"""
        self.mem.write(b'x', offset=0)


class TestSharedMemoryRemove(SharedMemoryTestBase):
    """Exercise mem.remove()"""
    def test_remove(self):
        """tests that mem.remove() works"""
        self.mem.detach()
        self.mem.remove()
        with self.assertRaises(sysv_ipc.ExistentialError):
            sysv_ipc.SharedMemory(self.mem.key)
        # Wipe this out so that self.tearDown() doesn't crash.
        self.mem = None


class TestSharedMemoryPropertiesAndAttributes(SharedMemoryTestBase):
    """Exercise props and attrs"""
    def test_property_key(self):
        """exercise SharedMemory.key"""
        self.assertGreaterEqual(self.mem.key, sysv_ipc.KEY_MIN)
        self.assertLessEqual(self.mem.key, sysv_ipc.KEY_MAX)
        self.assertWriteToReadOnlyPropertyFails('key', 42)

    def test_property_id(self):
        """exercise SharedMemory.id"""
        self.assertGreaterEqual(self.mem.id, 0)
        self.assertWriteToReadOnlyPropertyFails('id', 42)

    def test_property_size(self):
        """exercise SharedMemory.size"""
        self.assertGreaterEqual(self.mem.size, 0)
        self.assertWriteToReadOnlyPropertyFails('size', 42)

    def test_property_address(self):
        """exercise SharedMemory.address"""
        self.assertGreaterEqual(self.mem.address, 0)
        self.assertWriteToReadOnlyPropertyFails('address', 42)

    def test_property_attached(self):
        """exercise SharedMemory.attached"""
        self.assertTrue(self.mem.attached)
        self.assertWriteToReadOnlyPropertyFails('attached', False)
        self.mem.detach()
        self.assertFalse(self.mem.attached)

    def test_property_last_attach_time(self):
        """exercise SharedMemory.last_attach_time"""
        self.mem.detach()
        # I can't record exactly when this attach() happens, but as long as it is within 5 seconds
        # of the assertion happening, this test will pass.
        self.mem.attach()
        self.assertLess(self.mem.last_attach_time - time.time(), 5)
        self.assertWriteToReadOnlyPropertyFails('last_attach_time', 42)

    def test_property_last_detach_time(self):
        """exercise SharedMemory.last_detach_time"""
        # I can't record exactly when this attach() happens, but as long as it is within 5 seconds
        # of the assertion happening, this test will pass.
        self.mem.detach()
        self.assertLess(self.mem.last_detach_time - time.time(), 5)
        self.assertWriteToReadOnlyPropertyFails('last_detach_time', 42)

    def test_property_last_change_time(self):
        """exercise SharedMemory.last_change_time"""
        # I can't record exactly when this last_change_time is set, but as long as it is within
        # 5 seconds of the assertion happening, this test will pass.

        # BTW this is the most useless attribute ever:
        # "The last time a process changed the uid, gid or mode on this segment."
        self.mem.mode = 0x1ff  # = octal 777 which is expressed differently in Python 2 & 3
        self.assertLess(self.mem.last_change_time - time.time(), 5)
        self.assertWriteToReadOnlyPropertyFails('last_change_time', 42)

    def test_property_creator_pid(self):
        """exercise SharedMemory.creator_pid"""
        self.assertEqual(self.mem.creator_pid, os.getpid())
        self.assertWriteToReadOnlyPropertyFails('creator_pid', 42)

    def test_property_last_pid(self):
        """exercise SharedMemory.last_pid"""
        self.assertEqual(self.mem.last_pid, os.getpid())
        self.assertWriteToReadOnlyPropertyFails('last_pid', 42)

    def test_property_number_attached(self):
        """exercise SharedMemory.number_attached"""
        self.assertEqual(self.mem.number_attached, 1)
        self.mem.detach()
        self.assertEqual(self.mem.number_attached, 0)
        self.assertWriteToReadOnlyPropertyFails('number_attached', 42)

    def test_attribute_uid(self):
        """exercise SharedMemory.uid"""
        self.assertEqual(self.mem.uid, os.geteuid())

    def test_attribute_gid(self):
        """exercise SharedMemory.gid"""
        self.assertEqual(self.mem.gid, os.getgid())

    def test_attribute_cuid(self):
        """exercise SharedMemory.cuid"""
        self.assertEqual(self.mem.cuid, os.geteuid())
        self.assertWriteToReadOnlyPropertyFails('cuid', 42)

    def test_attribute_cgid(self):
        """exercise SharedMemory.cgid"""
        self.assertEqual(self.mem.cgid, os.getgid())
        self.assertWriteToReadOnlyPropertyFails('cgid', 42)

if __name__ == '__main__':
    unittest.main()

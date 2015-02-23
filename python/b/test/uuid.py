from b.uuid import UUID
import unittest

class TestMain(unittest.TestCase):
    def test_uuid_init(self):
        uuid = UUID(
            0x12345678,
            0x9ABC,
            0xDEF0,
            0x0000,
            0x000000000000,
        )
        data = uuid.to_list()
        # FIXME(strager): Bytes aren't ordered properly.
        self.assertEqual(0x12, data[0])
        self.assertEqual(0x34, data[1])
        self.assertEqual(0x56, data[2])
        self.assertEqual(0x78, data[3])
        self.assertEqual(0x9A, data[4])
        self.assertEqual(0xBC, data[5])
        self.assertEqual(0xDE, data[6])
        self.assertEqual(0xF0, data[7])
        self.assertEqual(0x00, data[8])

if __name__ == '__main__':
    unittest.main()

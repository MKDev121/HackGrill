import unittest
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "src"))
from ipc.shm_ring_buffer import ShmRingBuffer, SHM_MAGIC


class TestShmRingBuffer(unittest.TestCase):
    def test_shm_create_and_read_header(self):
        shm_name = "voip_pytest_shm_test"
        buffer = ShmRingBuffer(name=shm_name, capacity=64 * 1024, create=True)

        header = buffer.read_header()
        self.assertEqual(header["magic"], SHM_MAGIC)
        self.assertEqual(header["capacity"], 64 * 1024)
        self.assertEqual(header["write_pos"], 0)
        self.assertEqual(header["read_pos"], 0)

        # Write a test audio frame
        payload = b"\x01\x02\x03\x04" * 160  # 640 bytes (20ms at 16kHz PCM16)
        written = buffer.write_frame(payload, timestamp_us=12345, sequence_num=1)
        self.assertTrue(written)

        frame_res = buffer.read_frame()
        self.assertIsNotNone(frame_res)
        f_header, f_payload = frame_res
        self.assertEqual(f_header["sequence_num"], 1)
        self.assertEqual(f_payload, payload)

        buffer.close()


if __name__ == "__main__":
    unittest.main()

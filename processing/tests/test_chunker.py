import unittest
import numpy as np
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "src"))
from audio.chunker import AudioChunker


class TestAudioChunker(unittest.TestCase):
    def test_chunker_basic(self):
        chunker = AudioChunker(sample_rate=16000, chunk_duration_ms=200, vad_threshold=100.0)
        # 200ms = 3200 samples = 6400 bytes
        tone = (np.sin(np.linspace(0, 100, 3200)) * 10000).astype(np.int16)
        chunker.add_samples(tone.tobytes())

        chunk, is_final = chunker.get_next_chunk()
        self.assertIsNotNone(chunk)
        self.assertEqual(len(chunk), 3200)
        self.assertFalse(is_final)

    def test_chunker_insufficient_samples(self):
        chunker = AudioChunker(sample_rate=16000, chunk_duration_ms=200)
        short_data = np.zeros(1000, dtype=np.int16)
        chunker.add_samples(short_data.tobytes())

        result = chunker.get_next_chunk()
        self.assertIsNone(result)


if __name__ == "__main__":
    unittest.main()

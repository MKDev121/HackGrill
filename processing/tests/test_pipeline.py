import unittest
import numpy as np
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "src"))
from pipeline import TranslationPipeline


class TestPipeline(unittest.TestCase):
    def test_pipeline_english_target(self):
        results = []
        pipeline = TranslationPipeline(
            source_lang="hi-IN",
            target_lang="en-IN",
            chunk_size_ms=200,
            on_result_callback=lambda event: results.append(event)
        )

        samples = np.zeros(3200, dtype=np.int16)
        pipeline.process_audio_bytes(samples.tobytes())

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["type"], "TRANSCRIPT_EVENT")
        self.assertEqual(results[0]["target_language"], "en-IN")
        self.assertIn("latency", results[0])
        self.assertGreater(results[0]["latency"]["total_ms"], 0)

    def test_pipeline_indic_target(self):
        results = []
        pipeline = TranslationPipeline(
            source_lang="hi-IN",
            target_lang="ta-IN",
            chunk_size_ms=200,
            on_result_callback=lambda event: results.append(event)
        )

        samples = np.zeros(3200, dtype=np.int16)
        pipeline.process_audio_bytes(samples.tobytes())

        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["target_language"], "ta-IN")
        self.assertNotEqual(results[0]["translated_text"], "")


if __name__ == "__main__":
    unittest.main()

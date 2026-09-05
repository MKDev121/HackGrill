"""
Real-time audio buffer and stream chunker for low-latency ASR.
"""

import numpy as np
from typing import List, Optional, Tuple


class AudioChunker:
    def __init__(
        self,
        sample_rate: int = 16000,
        chunk_duration_ms: int = 300,
        vad_threshold: float = 300.0,
        max_buffer_ms: int = 2000,
    ):
        self.sample_rate = sample_rate
        self.chunk_duration_ms = chunk_duration_ms
        self.vad_threshold = vad_threshold
        self.max_buffer_ms = max_buffer_ms

        self.chunk_samples = int(sample_rate * (chunk_duration_ms / 1000.0))
        self.max_buffer_samples = int(sample_rate * (max_buffer_ms / 1000.0))
        self.buffer = np.array([], dtype=np.int16)

        self.silence_frames = 0
        self.speech_frames = 0

    def add_samples(self, pcm_bytes: bytes):
        if not pcm_bytes:
            return
        new_samples = np.frombuffer(pcm_bytes, dtype=np.int16)
        self.buffer = np.append(self.buffer, new_samples)

    def is_speech(self, samples: np.ndarray) -> bool:
        if len(samples) == 0:
            return False
        rms = np.sqrt(np.mean(samples.astype(np.float32) ** 2))
        return rms > self.vad_threshold

    def get_next_chunk(self) -> Optional[Tuple[np.ndarray, bool]]:
        """
        Returns (audio_chunk_array, is_final_chunk) if a valid chunk is ready,
        or None if insufficient samples.
        """
        if len(self.buffer) < self.chunk_samples:
            return None

        # Extract next chunk window
        chunk = self.buffer[:self.chunk_samples]
        speech_active = self.is_speech(chunk)

        if speech_active:
            self.speech_frames += 1
            self.silence_frames = 0
        else:
            self.silence_frames += 1

        is_final = False
        # Speech followed by silence indicates end of utterance
        if self.speech_frames > 0 and self.silence_frames >= 2:
            is_final = True
            self.speech_frames = 0
            self.silence_frames = 0

        # Advance buffer
        self.buffer = self.buffer[self.chunk_samples:]
        return chunk, is_final

    def flush(self) -> Optional[np.ndarray]:
        if len(self.buffer) == 0:
            return None
        rem = self.buffer
        self.buffer = np.array([], dtype=np.int16)
        return rem

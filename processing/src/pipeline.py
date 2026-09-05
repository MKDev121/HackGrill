"""
Orchestrates audio streaming -> ASR (Saaras) -> Machine Translation (Mayura) -> Event emission.
"""

import time
from typing import Optional, Dict, Any, Callable
import numpy as np

from models.saaras_asr import SaarasASR
from models.mayura_translate import MayuraTranslate
from audio.chunker import AudioChunker


class TranslationPipeline:
    def __init__(
        self,
        source_lang: str = "hi-IN",
        target_lang: str = "en-IN",
        chunk_size_ms: int = 300,
        vad_sensitivity: float = 300.0,
        on_result_callback: Optional[Callable[[Dict[str, Any]], None]] = None
    ):
        self.source_lang = source_lang
        self.target_lang = target_lang
        self.chunk_size_ms = chunk_size_ms
        self.on_result = on_result_callback

        self.chunker = AudioChunker(
            sample_rate=16000,
            chunk_duration_ms=chunk_size_ms,
            vad_threshold=vad_sensitivity
        )
        self.asr_model = SaarasASR()
        self.mt_model = MayuraTranslate()

    def update_config(self, source_lang: Optional[str] = None, target_lang: Optional[str] = None, chunk_ms: Optional[int] = None):
        if source_lang:
            self.source_lang = source_lang
        if target_lang:
            self.target_lang = target_lang
        if chunk_ms and chunk_ms != self.chunk_size_ms:
            self.chunk_size_ms = chunk_ms
            self.chunker = AudioChunker(sample_rate=16000, chunk_duration_ms=chunk_ms)

    def process_audio_bytes(self, pcm_bytes: bytes, speaker: str = "remote"):
        """
        Feeds new PCM audio bytes into chunker and processes when a window is full.
        """
        self.chunker.add_samples(pcm_bytes)
        while True:
            chunk_res = self.chunker.get_next_chunk()
            if not chunk_res:
                break
            chunk_samples, is_final = chunk_res
            self._process_chunk(chunk_samples, is_final, speaker)

    def _process_chunk(self, chunk: np.ndarray, is_final: bool, speaker: str):
        total_start = time.perf_counter()

        # Check if target is English -> Saaras does ASR + MT in 1 step!
        is_target_english = self.target_lang.lower().startswith("en")

        if is_target_english:
            asr_res = self.asr_model.process(
                chunk,
                source_lang=self.source_lang,
                target_lang=self.target_lang,
                mode="translate"
            )
            original_text = ""
            translated_text = asr_res["text"]
            mt_latency = 0.0
            asr_latency = asr_res["latency_ms"]
        else:
            # Target is non-English: Step 1 = Saaras transcribe -> Step 2 = Mayura translate
            asr_res = self.asr_model.process(
                chunk,
                source_lang=self.source_lang,
                target_lang=self.target_lang,
                mode="transcribe"
            )
            original_text = asr_res["text"]
            asr_latency = asr_res["latency_ms"]

            mt_res = self.mt_model.translate(
                original_text,
                source_lang=self.source_lang,
                target_lang=self.target_lang
            )
            translated_text = mt_res["translated_text"]
            mt_latency = mt_res["latency_ms"]

        total_latency_ms = (time.perf_counter() - total_start) * 1000.0 + asr_latency + mt_latency

        event = {
            "type": "TRANSCRIPT_EVENT",
            "speaker": speaker,
            "is_final": is_final,
            "original_text": original_text,
            "translated_text": translated_text,
            "source_language": self.source_lang,
            "target_language": self.target_lang,
            "latency": {
                "asr_ms": round(asr_latency, 2),
                "translation_ms": round(mt_latency, 2),
                "total_ms": round(total_latency_ms, 2)
            },
            "timestamp_us": int(time.time() * 1_000_000)
        }

        if self.on_result:
            self.on_result(event)

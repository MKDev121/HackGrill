"""
Python Processing App Entry Point for On-Device ASR & Translation.
"""

import sys
import time
import argparse
import signal
from pathlib import Path

# Add src to sys.path
sys.path.insert(0, str(Path(__file__).parent))

from ipc.shm_ring_buffer import ShmRingBuffer
from ipc.control_socket import ControlSocketServer
from pipeline import TranslationPipeline

running = True


def handle_signal(sig, frame):
    global running
    print(f"\n[ProcessingApp] Signal {sig} received. Shutting down...")
    running = False


def main():
    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    parser = argparse.ArgumentParser(description="VoIP On-Device Speech Translation Engine")
    parser.add_argument("--source-lang", default="hi-IN", help="Source language (e.g. hi-IN, te-IN, ta-IN)")
    parser.add_argument("--target-lang", default="en-IN", help="Target language (e.g. en-IN, hi-IN)")
    parser.add_argument("--chunk-ms", type=int, default=300, help="Chunk duration in milliseconds")
    parser.add_argument("--port", type=int, default=9201, help="Control socket IPC port")
    parser.add_argument("--shm-name", default="voip_audio_rx_shm", help="RX shared memory buffer identifier")
    args = parser.parse_args()

    print("========================================================")
    print("   VoIP Processing Engine (Sarvam Edge On-Device)        ")
    print(f"   Source: {args.source_lang} -> Target: {args.target_lang}")
    print(f"   Chunk Size: {args.chunk_ms} ms | IPC Port: {args.port}")
    print("========================================================\n")

    # 1. Start Control Socket Server for broadcasting transcript events
    control_server = ControlSocketServer(port=args.port)
    control_server.start()

    # 2. Initialize Translation Pipeline
    def on_transcript_result(event: dict):
        print(f"[Transcript] {event['source_language']} -> {event['target_language']}: "
              f"'{event['translated_text']}' (Latency: {event['latency']['total_ms']}ms)")
        control_server.broadcast(event)

    pipeline = TranslationPipeline(
        source_lang=args.source_lang,
        target_lang=args.target_lang,
        chunk_size_ms=args.chunk_ms,
        on_result_callback=on_transcript_result
    )

    # Handle incoming control commands
    def handle_control_msg(msg: dict):
        if msg.get("type") == "CONFIG_UPDATE":
            pipeline.update_config(
                source_lang=msg.get("source_language"),
                target_lang=msg.get("target_language"),
                chunk_ms=msg.get("chunk_size_ms")
            )
            print(f"[ProcessingApp] Updated config: {msg}")

    control_server.on_message_callback = handle_control_msg

    # 3. Attach to Shared Memory Ring Buffer
    print(f"[ProcessingApp] Attaching to SHM buffer '{args.shm_name}'...")
    shm_buffer = None
    retry_count = 0
    while running and not shm_buffer:
        try:
            shm_buffer = ShmRingBuffer(args.shm_name, create=False)
            print(f"[ProcessingApp] Successfully connected to SHM buffer '{args.shm_name}'")
        except Exception as e:
            retry_count += 1
            if retry_count % 5 == 0:
                print(f"[ProcessingApp] Waiting for transmission app to initialize SHM '{args.shm_name}'...")
            time.sleep(1.0)

    # 4. Main audio processing loop
    print("[ProcessingApp] Engine ready. Processing incoming audio frames...")
    while running and shm_buffer:
        try:
            frame = shm_buffer.read_frame()
            if frame:
                header, pcm_payload = frame
                pipeline.process_audio_bytes(pcm_payload, speaker="remote")
            else:
                time.sleep(0.005)  # 5ms yield when idle
        except Exception as e:
            print(f"[ProcessingApp] Error in processing loop: {e}")
            time.sleep(0.05)

    print("[ProcessingApp] Cleaning up...")
    if shm_buffer:
        shm_buffer.close()
    control_server.stop()
    print("[ProcessingApp] Shutdown complete.")


if __name__ == "__main__":
    main()

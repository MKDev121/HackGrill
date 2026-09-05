# Live VoIP App with Native On-Device Translation: Architectural Plan & Implementation Guide

## 1. Executive Summary & Goals

The **Live VoIP Translation App** is a desktop telecommunication application that provides real-time, bidirectional voice translation entirely **on-device** with zero reliance on cloud APIs.

### Primary Objectives
- **Zero Cloud / API Dependency**: Completely offline transcription and translation using Sarvam Edge models (<1 GB total model footprint).
- **Sub-500ms End-to-End Latency**: From the speaker's vocalization to the remote party receiving translated audio/subtitles.
- **High Concurrency & Modularity**: Multi-process OS architecture separating UI (Flutter), networking (C++), and AI inference (Python) over zero-overhead shared memory IPC.

---

## 2. End-to-End Latency Budget

To maintain a fluid conversation, the total latency budget is constrained to **< 500 ms**:

| Pipeline Stage | Component | Allocation | Optimization Strategy |
|---|---|---|---|
| **Audio Capture & Framing** | Flutter Audio Service | 20 ms | 16 kHz 16-bit PCM capture in 20ms frames |
| **TX IPC Passing** | Shared Memory Ring Buffer | < 1 ms | Lockless circular memory mapping |
| **Network Transit (One-Way)** | C++ UDP / RTP Engine | 20 – 50 ms | Raw UDP / RTP transport with jitter buffer |
| **Audio Chunking & VAD** | Python Processing Engine | 150 – 250 ms | 200–300ms sliding windows with energy VAD |
| **On-Device Inference** | Sarvam Edge (Saaras / Mayura) | 120 – 180 ms | Quantized ONNX / NPU / GPU acceleration |
| **IPC Event & UI Rendering** | Control Socket + Flutter UI | 5 – 10 ms | Length-prefixed binary socket dispatch |
| **Total Estimated Latency** | | **~315 – 490 ms** | **Within target < 500ms budget** |

---

## 3. System Architecture & Components

```
+-----------------------------------------------------------------------------------------+
|                                    LOCAL DESKTOP MACHINE                                |
|                                                                                         |
|   +--------------------------+                         +----------------------------+   |
|   |  Flutter Desktop UI      |                         |  C++ Transmission Engine   |   |
|   |  - Call screen           | <--- Control Socket --> |  - UDP / RTP Networking    |   |
|   |  - Audio I/O capture/spk |        (Port 9200)      |  - Sender / Receiver thread|   |
|   |  - Live Subtitles View   |                         |  - Network telemetry / RTT |   |
|   +--------------------------+                         +----------------------------+   |
|                 ^                                                     ^                 |
|                 | (Live Event Stream)                                 | (RTP In/Out)    |
|                 v                                                     v                 |
|   +---------------------------------------------------------------------------------+   |
|   |                            Shared Memory Ring Buffers                           |   |
|   |   - voip_audio_tx_shm: Mic audio -> Network transmission & local ASR            |   |
|   |   - voip_audio_rx_shm: Incoming remote RTP audio -> ASR processing & speaker    |   |
|   +---------------------------------------------------------------------------------+   |
|                                         ^                                               |
|                                         | (Audio stream read/write)                     |
|                                         v                                               |
|   +---------------------------------------------------------------------------------+   |
|   |                        Python Processing Daemon (Sarvam Edge)                   |   |
|   |   - AudioChunker: Real-time windowing & VAD speech segmentation                 |   |
|   |   - SaarasASR: Single-pass translation to English OR source transcription       |   |
|   |   - MayuraTranslate: Indic-to-Indic / Indic-to-English Neural Machine Trans     |   |
|   +---------------------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------------------+
```

---

## 4. Key Architectural Decisions

### 4.1 Plain OS Processes (No Docker)
Running as standalone native processes on the host eliminates container virtualization overhead, minimizes IPC friction, and grants direct access to hardware audio drivers and local GPU/NPU acceleration.

### 4.2 Combined C++ Transmission Process
Handling both outgoing (TX) and incoming (RX) audio within a single C++ process allows:
- Sharing the active UDP socket session and NAT traversal state.
- Accurately synchronizing round-trip time (RTT), jitter estimation, and RTCP packet loss statistics.
- Running high-priority sender and receiver threads concurrently without cross-process locking.

### 4.3 Sarvam Edge Model Execution Pathways
- **Target Language = English (`en-IN`)**:
  - `Saaras` executes in direct `translate` mode.
  - ASR + MT are accomplished in a **single model inference pass**.
- **Target Language = Indic (`hi-IN`, `ta-IN`, `te-IN`, `mr-IN`, etc.)**:
  - `Saaras` transcribes audio to source text (`transcribe` mode).
  - `Mayura` performs on-device machine translation to the destination Indic language.

---

## 5. Inter-Process Communication (IPC) Design

### 5.1 Shared Memory Ring Buffer
- **Capacity**: 1 MB (~32 seconds of 16kHz 16-bit mono audio).
- **Zero-Copy**: Audio frames are written with an atomic write pointer and read with an atomic read pointer.
- **Cross-Platform**: Uses POSIX `shm_open`/`mmap` on Linux/macOS and `CreateFileMapping`/`MapViewOfFile` on Windows.

### 5.2 Control & Telemetry Sockets
- **Port 9200**: C++ Transmission Engine control server.
- **Port 9201**: Python Processing Daemon telemetry/transcript broadcast server.
- **Framing**: 4-byte big-endian length prefix followed by JSON / Protobuf payload.

---

## 6. Build and Run Workflow

1. **Build All Components**:
   ```bash
   # Unix / macOS
   ./scripts/build_all.sh

   # Windows PowerShell
   .\scripts\build_all.ps1
   ```

2. **Launch Application**:
   ```bash
   # Unix / macOS
   ./scripts/run_local.sh

   # Windows PowerShell
   .\scripts\run_local.ps1
   ```

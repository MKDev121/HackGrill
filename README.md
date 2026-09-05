# Live VoIP App with Native On-Device Translation

A live desktop VoIP telecommunication application that performs real-time speech transcription and translation entirely **on-device** using the **Sarvam Edge** stack with zero cloud API dependencies.

---

## Project Structure

```
voip-translation-app/
├── frontend/                          # Flutter desktop app
│   ├── lib/
│   │   ├── main.dart                  # App initialization and theme setup
│   │   ├── screens/
│   │   │   ├── call_screen.dart       # Live call screen with subtitles waterfall & metrics
│   │   │   └── settings_screen.dart   # Language pairs, audio I/O & network config
│   │   ├── services/
│   │   │   ├── audio_service.dart     # Mic capture & speaker playback service
│   │   │   └── ipc_client.dart        # Talks to backend engines via IPC socket
│   │   └── models/
│   │       └── call_state.dart        # Session, transcript, and telemetry models
│   ├── pubspec.yaml
│   └── (platform folders: windows/, linux/, macos/)
│
├── transmission/                      # C++ app — combined send/receive
│   ├── src/
│   │   ├── main.cpp                   # Transmission engine lifecycle & IPC server
│   │   ├── network/
│   │   │   ├── connection.cpp/.h      # Bidirectional UDP/RTP socket handling
│   │   │   ├── receiver.cpp/.h        # Incoming audio thread & packet unpacker
│   │   │   └── sender.cpp/.h          # Outgoing audio thread & packetizer
│   │   ├── ipc/
│   │   │   ├── shm_ring_buffer.cpp/.h # Shared memory ring buffer (audio stream)
│   │   │   └── control_socket.cpp/.h  # Control message socket server
│   │   └── audio/
│   │       └── codec.cpp/.h           # PCM16 framing, VAD, and codec utilities
│   ├── include/
│   │   └── transmission/common.h      # Shared data structures and constants
│   ├── CMakeLists.txt
│   └── tests/
│       ├── test_shm_buffer.cpp
│       ├── test_network.cpp
│       └── CMakeLists.txt
│
├── processing/                        # Python app(s) — ASR + translation
│   ├── src/
│   │   ├── main.py                    # Processing daemon entry point
│   │   ├── pipeline.py                # Orchestrates ASR -> (translation)
│   │   ├── models/
│   │   │   ├── saaras_asr.py          # Sarvam Edge Saaras wrapper
│   │   │   └── mayura_translate.py    # Sarvam Edge Mayura wrapper (if non-English)
│   │   ├── ipc/
│   │   │   ├── shm_ring_buffer.py     # Python wrapper matching C++ SHM layout
│   │   │   └── control_socket.py      # Socket client / server for IPC
│   │   └── audio/
│   │       └── chunker.py             # Buffering & VAD chunking logic
│   ├── requirements.txt
│   └── tests/
│       ├── test_chunker.py
│       ├── test_pipeline.py
│       └── test_shm.py
│
├── shared/                            # Cross-language contracts
│   ├── ipc_protocol.md                # Message format, chunk size, buffer layout
│   └── proto/
│       └── control_messages.proto     # Protobuf schema for control & telemetry
│
├── scripts/
│   ├── build_all.sh                   # Linux/macOS build script
│   ├── run_local.sh                   # Linux/macOS run script
│   ├── build_all.ps1                  # Windows build script
│   └── run_local.ps1                  # Windows run script
│
├── docs/
│   └── voip-translation-app-plan.md   # Architectural design and latency budget plan
│
└── README.md
```

---

## Architecture Overview

1. **Frontend (Flutter Desktop)**: Handles UI, user controls, audio I/O capture/playback, and live subtitle display.
2. **Transmission Engine (C++)**: High-performance, single-process engine managing bidirectional RTP/UDP networking with dedicated sender and receiver threads.
3. **Processing Engine (Python)**: Real-time on-device ASR and translation using Sarvam Edge models (`Saaras` for ASR and English translation; `Mayura` for Indic-to-Indic translation).
4. **Inter-Process Communication (IPC)**:
   - **Audio Stream**: Lockless Shared Memory Ring Buffer (`voip_audio_tx_shm` & `voip_audio_rx_shm`).
   - **Control / Subtitles**: Length-prefixed binary socket / named pipe on localhost ports `9200` (Transmission) and `9201` (Processing).

---

## Quick Start

### Prerequisites
- **C++ Compiler**: GCC/Clang with C++17 support or MSVC 2019+
- **CMake**: >= 3.16
- **Python**: >= 3.9
- **Flutter**: >= 3.0.0

### Build All Components
```bash
# On Linux / macOS
./scripts/build_all.sh

# On Windows (PowerShell)
.\scripts\build_all.ps1
```

### Run Locally
```bash
# On Linux / macOS
./scripts/run_local.sh

# On Windows (PowerShell)
.\scripts\run_local.ps1
```

---

## Running Unit Tests

### C++ Transmission Tests
```bash
cd transmission/build
ctest --output-on-failure
```

### Python Processing Tests
```bash
cd processing
pytest tests/
```

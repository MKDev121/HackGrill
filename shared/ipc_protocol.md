# Inter-Process Communication (IPC) Protocol Specification

This document defines the cross-process communication contract between the three components of the **VoIP Translation App**:
1. **Frontend (Flutter Desktop)**: UI, Audio I/O, Call Control
2. **Transmission Engine (C++)**: Bidirectional RTP/UDP Networking, Audio Packetization
3. **Processing Engine (Python)**: Real-Time ASR (Sarvam Edge Saaras) + Translation (Sarvam Edge Mayura)

---

## 1. Architecture Overview

```
+-----------------------------------------------------------------------------+
|                               Local Machine                                 |
|                                                                             |
|   +--------------------+               +--------------------------------+   |
|   |  Flutter Frontend  | <--- IPC ---> |     C++ Transmission App       |   |
|   | (UI / Audio I/O)   | (Control Sock)| (Send/Receive RTP & Networking)|   |
|   +--------------------+               +--------------------------------+   |
|            ^                                       ^                        |
|            | (Live Subtitles / Control)            |                        |
|            v                                       v                        |
|   +---------------------------------------------------------------------+   |
|   |                       Shared Memory Ring Buffer                     |   |
|   |       - voip_audio_tx_shm (Outgoing Mic Audio -> Network)           |   |
|   |       - voip_audio_rx_shm (Incoming Net Audio -> ASR -> Spkr)       |   |
|   +---------------------------------------------------------------------+   |
|                                    ^                                        |
|                                    | (Audio Stream In/Out)                  |
|                                    v                                        |
|   +---------------------------------------------------------------------+   |
|   |                        Python Processing App                        |   |
|   |         (Sarvam Edge: Saaras ASR -> Mayura Translation Engine)      |   |
|   +---------------------------------------------------------------------+   |
+-----------------------------------------------------------------------------+
```

---

## 2. Shared Memory Ring Buffer (Audio Data Path)

Audio is transferred using shared memory (`shm_open` on POSIX / `CreateFileMapping` on Windows) to avoid IPC copying overhead and achieve single-digit millisecond latency.

### 2.1 Shared Memory Regions
| Region Name | Identifier | Purpose | Default Capacity |
|-------------|------------|---------|------------------|
| **TX Buffer** | `voip_audio_tx_shm` | Raw mic audio sent from audio capture to transmission & local ASR | 1 MB (~32 seconds of 16kHz 16-bit PCM) |
| **RX Buffer** | `voip_audio_rx_shm` | Incoming network audio from remote peer fed into ASR & speaker | 1 MB |
| **Processed Buffer** | `voip_audio_proc_shm` | Translated/synthesized audio (if TTS active) | 1 MB |

### 2.2 Memory Layout

```
+-------------------------------------------------------------------+
| RingBufferHeader (64 bytes)                                      |
| - magic (4 bytes): 0x53484D52 ('SHMR')                            |
| - version (4 bytes): 1                                            |
| - capacity (8 bytes): 1048576 bytes                               |
| - write_pos (8 bytes, atomic): offset in ring                     |
| - read_pos (8 bytes, atomic): offset in ring                      |
| - sample_rate (4 bytes): 16000                                    |
| - channels (2 bytes): 1 (Mono)                                    |
| - bit_depth (2 bytes): 16 (16-bit Signed LE PCM)                  |
| - sequence (8 bytes): packet count counter                        |
| - reserved (16 bytes): alignment padding                          |
+-------------------------------------------------------------------+
| Circular Audio Buffer Area (capacity bytes)                       |
| [Frame 1][Frame 2][Frame 3] ... [Frame N]                         |
+-------------------------------------------------------------------+
```

### 2.3 Audio Frame Header (Prepended to each chunk in buffer)
```c
struct AudioFrameHeader {
    uint32_t magic;         // 0x4652414D ('FRAM')
    uint64_t timestamp_us;  // Microsecond timestamp (monotonic)
    uint64_t sequence_num;   // Monotonically increasing frame sequence
    uint32_t payload_size;  // Size of PCM audio chunk in bytes (e.g. 640 bytes = 20ms)
    uint16_t sample_rate;   // 16000
    uint8_t  channels;      // 1
    uint8_t  flags;         // Bit 0: VAD active, Bit 1: Discontinuous/Muted
};
```

---

## 3. Control Socket Protocol (Control & Telemetry Path)

Control messages, telemetry, and live transcription results are exchanged over local domain sockets / named pipes / TCP `localhost:9200` (Transmission) and `localhost:9201` (Processing).

### 3.1 Framing
Each message is framed with a 4-byte big-endian length prefix followed by a JSON payload (or Protobuf payload):
```
+---------------------------+-----------------------------------------------+
| Length (4 bytes, uint32)  | Payload (JSON string or Protobuf binary)      |
+---------------------------+-----------------------------------------------+
```

### 3.2 Message Schemas (JSON)

#### 1. Session Init (`SESSION_INIT`)
Sent by Frontend to start a VoIP session.
```json
{
  "type": "SESSION_INIT",
  "session_id": "call-uuid-12345",
  "remote_host": "192.168.1.50",
  "remote_port": 5004,
  "local_port": 5004,
  "codec": "PCM16",
  "source_language": "hi-IN",
  "target_language": "en-IN",
  "translation_mode": "translate"
}
```

#### 2. Translation Config Update (`CONFIG_UPDATE`)
```json
{
  "type": "CONFIG_UPDATE",
  "source_language": "te-IN",
  "target_language": "en-IN",
  "chunk_size_ms": 300,
  "vad_sensitivity": 0.6
}
```

#### 3. Real-Time Transcript Event (`TRANSCRIPT_EVENT`)
Emitted by Python Processing App to Frontend.
```json
{
  "type": "TRANSCRIPT_EVENT",
  "session_id": "call-uuid-12345",
  "speaker": "remote",
  "is_final": true,
  "original_text": "नमस्ते, आप कैसे हैं?",
  "translated_text": "Hello, how are you?",
  "source_language": "hi-IN",
  "target_language": "en-IN",
  "latency": {
    "asr_ms": 142.5,
    "translation_ms": 88.2,
    "total_ms": 230.7
  },
  "timestamp_us": 1725514800000000
}
```

#### 4. Call State & Metrics (`METRICS_UPDATE`)
Emitted by C++ Transmission App.
```json
{
  "type": "METRICS_UPDATE",
  "packets_sent": 1450,
  "packets_received": 1448,
  "packet_loss_rate": 0.0013,
  "jitter_ms": 4.2,
  "round_trip_ms": 32.0,
  "audio_bitrate_kbps": 256.0
}
```

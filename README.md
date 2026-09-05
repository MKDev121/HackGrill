# Live VOIP App with Native On-Device Translation

## Idea
A live VOIP desktop application that performs real-time speech translation entirely on-device — no cloud/API calls for transcription or translation.

## Architecture Overview (Updated)

**Frontend:** Flutter desktop app
- Handles UI, call controls, audio I/O capture/playback

**Backend:** 2 applications, plain OS processes (no Docker), same physical device

| App | Function | Tech |
|-----|----------|------|
| Transmission app | Handles both incoming and outgoing audio/data on the same connection (bidirectional, single process, separate threads for send/receive) | C++ |
| Processing app(s) | ASR + translation using Sarvam Edge on-device (Saaras + Mayura) | Python |

## Tech Stack
- Flutter (frontend)
- C++ (single transmission app — combined send/receive)
- Python (processing app — ASR + translation)
- Sarvam Edge on-device stack (no external API)

## Key Decisions Made

**1. No Docker**
Running as plain OS processes rather than containers — lower latency, simpler debugging. Docker can be added later for deployment/isolation if needed, without changing pipeline logic.

**2. Combined C++ transmission app (was stages 1 & 4)**
Send and receive share the same network connection in a live call — combining them into one process avoids syncing session/connection state across two separate processes via IPC.

**3. Python processing — depends on target language**
- **Target = English:** Saaras alone, in `translate` output mode, does ASR + translation in a single model call → **1 Python app/model**.
- **Target = non-English:** Saaras (transcribe) → Mayura (translate) as two model calls → can still run as **1 Python process** (both models loaded together, <1GB combined) or **2 separate apps** for isolation. This is an isolation/maintainability choice, not a latency one.
- *(Still to confirm: which language pair.)*

**4. Sarvam model choice: Sarvam Edge, not the cloud API models**
- Sarvam Edge (Saaras + Mayura + Bulbul, <1GB total) is built specifically for on-device deployment — matches the "no API" requirement.
- Sarvam-105B / Sarvam-Translate / cloud Saaras v3/v4 are API-based — not applicable here.

**5. App-count vs latency**
Confirmed: splitting into separate processes vs combining does **not** meaningfully affect latency. IPC (socket/shared-memory) between local processes is single-digit ms; model inference time dominates. Process structure is chosen for isolation/maintainability, not performance.

## Inter-Process Communication (IPC)
- **Audio stream data:** shared-memory ring buffer (lowest latency for continuous audio)
- **Control messages:** Unix domain sockets or pipes (native to both C++ and Python)
- Avoid HTTP/REST/JSON for the audio path — too much overhead per chunk

## Key Technical Risks / Open Questions
1. **End-to-end latency** — capture → ASR → (MT) → send needs to land under ~500ms–1s for a "live" feel. Must benchmark Saaras/Mayura inference latency in isolation before building the rest.
2. **Audio chunking strategy** — chunk/buffer size is the biggest latency vs. accuracy lever.
3. **Target language pair** — determines whether processing is 1 model call (→English) or 2 (→other language), which shapes the Python app structure.
4. **Hardware requirements** — validate real-time inference speed for Saaras/Mayura on target deployment hardware (GPU vs CPU-only).

## Current Status
Architecture simplified to 2 core backend apps (C++ transmission, Python processing). Sarvam Edge selected as the model stack. Target language pair and on-device inference benchmarks still pending.

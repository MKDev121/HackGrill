# Live VOIP App with Native On-Device Translation

## Idea
A live VOIP desktop application that performs real-time speech translation entirely on-device — no cloud/API calls for transcription or translation.

## Architecture Overview

**Frontend:** Flutter desktop app
- Handles UI, call controls, audio I/O capture/playback

**Backend:** 4-stage pipeline, all running on the same physical device as separate processes

| Stage | Function | Tech |
|-------|----------|------|
| 1 | Receive incoming audio/data (transmission in) | C++ |
| 2 | Transcription generation (ASR) | Python + Sarvam model (on-device) |
| 3 | Translation | Python + Sarvam model (on-device) |
| 4 | Send outgoing audio/data (transmission out) | C++ |

## Tech Stack
- Flutter (frontend)
- C++ (transmission stages 1 & 4)
- Python (processing stages 2 & 3)
- Sarvam model (on-device ASR + translation, no external API)

## Deployment Decision: No Docker
Running all 4 stages as plain OS processes rather than containers, for a real-time single-device app.

- **Pros of skipping Docker:** lower latency (no container network overhead), simpler debugging, no added ops complexity
- **Trade-off accepted:** dependency management (C++ build env + Python env + Sarvam deps) handled manually on one host
- Docker can be added later post-hoc if deployment portability or dependency isolation becomes necessary — doesn't require changing pipeline logic

## Inter-Process Communication (IPC)
- **Audio stream data:** shared-memory ring buffer (lowest latency for continuous audio)
- **Control messages between stages:** Unix domain sockets or pipes (natively supported by both C++ and Python)
- Avoid HTTP/REST/JSON for the audio path — too much overhead per chunk

## Key Technical Risks / Open Questions
1. **End-to-end latency** — Sequential pipeline (capture → ASR → MT → send) needs to land under ~500ms–1s for a "live" feel. ASR + translation via Sarvam are the heaviest links; must benchmark stage 2/3 latency in isolation before building the rest.
2. **Audio chunking strategy** — Chunk/buffer size is the biggest latency vs. accuracy lever; ASR needs a minimum context window, but larger chunks add delay.
3. **Sarvam model specifics** — Confirm whether ASR and translation are one model call or two, and get documented/benchmarked inference times on target hardware (GPU vs CPU-only).
4. **Hardware requirements** — On-device inference for both ASR and MT in real time may require a GPU; needs validation on target deployment hardware.

## Current Status
Architecture defined; Sarvam model local inference speed not yet validated.

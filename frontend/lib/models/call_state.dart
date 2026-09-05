import 'package:flutter/foundation.dart';

enum CallStatus {
  idle,
  connecting,
  connected,
  ended,
  error,
}

enum TranslationMode {
  translateToEnglish,
  translateIndicToIndic,
  transcribeOnly,
}

class LatencyMetrics {
  final double asrMs;
  final double translationMs;
  final double totalMs;

  const LatencyMetrics({
    this.asrMs = 0.0,
    this.translationMs = 0.0,
    this.totalMs = 0.0,
  });

  factory LatencyMetrics.fromJson(Map<String, dynamic> json) {
    return LatencyMetrics(
      asrMs: (json['asr_ms'] as num?)?.toDouble() ?? 0.0,
      translationMs: (json['translation_ms'] as num?)?.toDouble() ?? 0.0,
      totalMs: (json['total_ms'] as num?)?.toDouble() ?? 0.0,
    );
  }
}

class LiveTranscript {
  final String id;
  final String speaker; // 'local' or 'remote'
  final String originalText;
  final String translatedText;
  final String sourceLanguage;
  final String targetLanguage;
  final bool isFinal;
  final LatencyMetrics latency;
  final DateTime timestamp;

  LiveTranscript({
    required this.id,
    required this.speaker,
    required this.originalText,
    required this.translatedText,
    required this.sourceLanguage,
    required this.targetLanguage,
    this.isFinal = true,
    this.latency = const LatencyMetrics(),
    DateTime? timestamp,
  }) : timestamp = timestamp ?? DateTime.now();

  factory LiveTranscript.fromJson(Map<String, dynamic> json) {
    return LiveTranscript(
      id: json['session_id'] ?? DateTime.now().millisecondsSinceEpoch.toString(),
      speaker: json['speaker'] ?? 'remote',
      originalText: json['original_text'] ?? '',
      translatedText: json['translated_text'] ?? '',
      sourceLanguage: json['source_language'] ?? 'hi-IN',
      targetLanguage: json['target_language'] ?? 'en-IN',
      isFinal: json['is_final'] ?? true,
      latency: json['latency'] != null ? LatencyMetrics.fromJson(json['latency']) : const LatencyMetrics(),
      timestamp: DateTime.now(),
    );
  }
}

class NetworkStats {
  final int packetsSent;
  final int packetsReceived;
  final double packetLossRate;
  final double jitterMs;
  final double rttMs;
  final double audioBitrateKbps;

  const NetworkStats({
    this.packetsSent = 0,
    this.packetsReceived = 0,
    this.packetLossRate = 0.0,
    this.jitterMs = 0.0,
    this.rttMs = 0.0,
    this.audioBitrateKbps = 256.0,
  });

  factory NetworkStats.fromJson(Map<String, dynamic> json) {
    return NetworkStats(
      packetsSent: json['packets_sent'] ?? 0,
      packetsReceived: json['packets_received'] ?? 0,
      packetLossRate: (json['packet_loss_rate'] as num?)?.toDouble() ?? 0.0,
      jitterMs: (json['jitter_ms'] as num?)?.toDouble() ?? 0.0,
      rttMs: (json['round_trip_ms'] as num?)?.toDouble() ?? 0.0,
      audioBitrateKbps: (json['audio_bitrate_kbps'] as num?)?.toDouble() ?? 256.0,
    );
  }
}

class CallSessionConfig {
  String remoteHost;
  int remotePort;
  int localPort;
  String sourceLanguage;
  String targetLanguage;
  int chunkDurationMs;
  bool isMicMuted;
  bool isSpeakerMuted;

  CallSessionConfig({
    this.remoteHost = '127.0.0.1',
    this.remotePort = 5006,
    this.localPort = 5004,
    this.sourceLanguage = 'hi-IN',
    this.targetLanguage = 'en-IN',
    this.chunkDurationMs = 300,
    this.isMicMuted = false,
    this.isSpeakerMuted = false,
  });
}

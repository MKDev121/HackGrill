import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';
import 'package:flutter/foundation.dart';
import '../models/call_state.dart';

class IpcClient extends ChangeNotifier {
  Socket? _transSocket;
  Socket? _procSocket;

  bool _isTransConnected = false;
  bool _isProcConnected = false;

  final _transcriptController = StreamController<LiveTranscript>.broadcast();
  final _metricsController = StreamController<NetworkStats>.broadcast();

  Stream<LiveTranscript> get transcriptStream => _transcriptController.stream;
  Stream<NetworkStats> get metricsStream => _metricsController.stream;

  bool get isConnected => _isTransConnected || _isProcConnected;

  Future<void> connect({
    String host = '127.0.0.1',
    int transPort = 9200,
    int procPort = 9201,
  }) async {
    // 1. Connect to Transmission Engine (C++)
    try {
      _transSocket = await Socket.connect(host, transPort, timeout: const Duration(seconds: 2));
      _isTransConnected = true;
      _listenToSocket(_transSocket!, isProc: false);
      if (kDebugMode) print("[IpcClient] Connected to Transmission C++ Engine");
    } catch (e) {
      if (kDebugMode) print("[IpcClient] Transmission connection failed: $e");
    }

    // 2. Connect to Processing Engine (Python)
    try {
      _procSocket = await Socket.connect(host, procPort, timeout: const Duration(seconds: 2));
      _isProcConnected = true;
      _listenToSocket(_procSocket!, isProc: true);
      if (kDebugMode) print("[IpcClient] Connected to Processing Python Engine");
    } catch (e) {
      if (kDebugMode) print("[IpcClient] Processing connection failed: $e");
    }

    notifyListeners();
  }

  void _listenToSocket(Socket socket, {required bool isProc}) {
    final buffer = <int>[];

    socket.listen(
      (data) {
        buffer.addAll(data);
        while (buffer.length >= 4) {
          final length = ByteData.sublistView(Uint8List.fromList(buffer.sublist(0, 4))).getUint32(0, Endian.big);
          if (buffer.length < 4 + length) break;

          final payloadBytes = buffer.sublist(4, 4 + length);
          buffer.removeRange(0, 4 + length);

          try {
            final jsonStr = utf8.decode(payloadBytes);
            final msg = jsonDecode(jsonStr) as Map<String, dynamic>;
            _handleIncomingMessage(msg);
          } catch (e) {
            if (kDebugMode) print("[IpcClient] Parse error: $e");
          }
        }
      },
      onDone: () {
        if (isProc) _isProcConnected = false;
        else _isTransConnected = false;
        notifyListeners();
      },
      onError: (err) {
        if (isProc) _isProcConnected = false;
        else _isTransConnected = false;
        notifyListeners();
      },
    );
  }

  void _handleIncomingMessage(Map<String, dynamic> msg) {
    final type = msg['type'];
    if (type == 'TRANSCRIPT_EVENT') {
      final transcript = LiveTranscript.fromJson(msg);
      _transcriptController.add(transcript);
    } else if (type == 'METRICS_UPDATE') {
      final metrics = NetworkStats.fromJson(msg);
      _metricsController.add(metrics);
    }
  }

  void sendSessionInit(CallSessionConfig config) {
    final msg = {
      'type': 'SESSION_INIT',
      'remote_host': config.remoteHost,
      'remote_port': config.remotePort,
      'local_port': config.localPort,
      'source_language': config.sourceLanguage,
      'target_language': config.targetLanguage,
      'chunk_size_ms': config.chunkDurationMs,
    };
    _sendToSocket(_transSocket, msg);
    _sendToSocket(_procSocket, {
      'type': 'CONFIG_UPDATE',
      'source_language': config.sourceLanguage,
      'target_language': config.targetLanguage,
      'chunk_size_ms': config.chunkDurationMs,
    });
  }

  void updateLanguagePair(String sourceLang, String targetLang) {
    final msg = {
      'type': 'CONFIG_UPDATE',
      'source_language': sourceLang,
      'target_language': targetLang,
    };
    _sendToSocket(_procSocket, msg);
  }

  void _sendToSocket(Socket? socket, Map<String, dynamic> data) {
    if (socket == null) return;
    try {
      final payload = utf8.encode(jsonEncode(data));
      final header = ByteData(4)..setUint32(0, payload.length, Endian.big);
      socket.add(header.buffer.asUint8List() + payload);
    } catch (e) {
      if (kDebugMode) print("[IpcClient] Send error: $e");
    }
  }

  void disconnect() {
    _transSocket?.destroy();
    _procSocket?.destroy();
    _transSocket = null;
    _procSocket = null;
    _isTransConnected = false;
    _isProcConnected = false;
    notifyListeners();
  }

  @override
  void dispose() {
    disconnect();
    _transcriptController.close();
    _metricsController.close();
    super.dispose();
  }
}

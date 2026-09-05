import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import '../models/call_state.dart';
import '../services/ipc_client.dart';
import '../services/audio_service.dart';
import 'settings_screen.dart';

class CallScreen extends StatefulWidget {
  final IpcClient ipcClient;
  final AudioService audioService;

  const CallScreen({
    Key? key,
    required this.ipcClient,
    required this.audioService,
  }) : super(key: key);

  @override
  State<CallScreen> createState() => _CallScreenState();
}

class _CallScreenState extends State<CallScreen> {
  CallStatus _status = CallStatus.idle;
  final List<LiveTranscript> _transcripts = [];
  NetworkStats _stats = const NetworkStats();
  final ScrollController _scrollController = ScrollController();

  CallSessionConfig _config = CallSessionConfig();
  bool _isMuted = false;
  bool _isDeafened = false;

  @override
  void initState() {
    super.initState();
    _subscribeToStreams();
  }

  void _subscribeToStreams() {
    widget.ipcClient.transcriptStream.listen((event) {
      if (!mounted) return;
      setState(() {
        _transcripts.add(event);
      });
      _scrollToBottom();
    });

    widget.ipcClient.metricsStream.listen((metrics) {
      if (!mounted) return;
      setState(() {
        _stats = metrics;
      });
    });
  }

  void _scrollToBottom() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_scrollController.hasClients) {
        _scrollController.animateTo(
          _scrollController.position.maxScrollExtent,
          duration: const Duration(milliseconds: 250),
          curve: Curves.easeOut,
        );
      }
    });
  }

  void _toggleCall() {
    if (_status == CallStatus.connected || _status == CallStatus.connecting) {
      setState(() {
        _status = CallStatus.ended;
      });
      widget.audioService.stopCapture();
      Future.delayed(const Duration(milliseconds: 500), () {
        if (mounted) setState(() => _status = CallStatus.idle);
      });
    } else {
      setState(() {
        _status = CallStatus.connecting;
      });
      widget.ipcClient.connect().then((_) {
        widget.ipcClient.sendSessionInit(_config);
        widget.audioService.startCapture();
        if (mounted) {
          setState(() {
            _status = CallStatus.connected;
          });
        }
      });
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D1117),
      appBar: AppBar(
        backgroundColor: const Color(0xFF161B22),
        elevation: 0,
        title: Row(
          children: [
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
              decoration: BoxDecoration(
                gradient: const LinearGradient(colors: [Color(0xFF6366F1), Color(0xFF8B5CF6)]),
                borderRadius: BorderRadius.circular(6),
              ),
              child: const Text('Sarvam Edge', style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Colors.white)),
            ),
            const SizedBox(width: 12),
            const Text('VoIP Live Translation', style: TextStyle(fontSize: 16, fontWeight: FontWeight.w600, color: Colors.white)),
          ],
        ),
        actions: [
          _buildLanguageChip(),
          const SizedBox(width: 8),
          IconButton(
            icon: const Icon(Icons.settings, color: Colors.white70),
            onPressed: () {
              Navigator.of(context).push(
                MaterialPageRoute(
                  builder: (context) => SettingsScreen(
                    config: _config,
                    audioService: widget.audioService,
                    onConfigChanged: (newConfig) {
                      setState(() => _config = newConfig);
                      widget.ipcClient.updateLanguagePair(_config.sourceLanguage, _config.targetLanguage);
                    },
                  ),
                ),
              );
            },
          ),
          const SizedBox(width: 12),
        ],
      ),
      body: Column(
        children: [
          _buildMetricsBar(),
          Expanded(child: _buildTranscriptArea()),
          _buildBottomControlPanel(),
        ],
      ),
    );
  }

  Widget _buildLanguageChip() {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: const Color(0xFF21262D),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: const Color(0xFF30363D)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          const Icon(Icons.translate, size: 14, color: Color(0xFF58A6FF)),
          const SizedBox(width: 6),
          Text(
            '${_config.sourceLanguage} -> ${_config.targetLanguage}',
            style: const TextStyle(fontSize: 12, color: Color(0xFFC9D1D9), fontWeight: FontWeight.w500),
          ),
        ],
      ),
    );
  }

  Widget _buildMetricsBar() {
    final isOnline = _status == CallStatus.connected;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 20, vertical: 8),
      color: const Color(0xFF161B22).withOpacity(0.6),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Row(
            children: [
              Container(
                width: 8,
                height: 8,
                decoration: BoxDecoration(
                  shape: BoxShape.circle,
                  color: isOnline ? const Color(0xFF3FB950) : const Color(0xFFF85149),
                ),
              ),
              const SizedBox(width: 8),
              Text(
                isOnline ? 'LIVE CALL ACTIVE' : 'STANDBY',
                style: TextStyle(
                  fontSize: 11,
                  fontWeight: FontWeight.bold,
                  letterSpacing: 0.5,
                  color: isOnline ? const Color(0xFF3FB950) : const Color(0xFF8B949E),
                ),
              ),
            ],
          ),
          Row(
            children: [
              _buildMetricItem('RTT', '${_stats.rttMs.toStringAsFixed(1)} ms'),
              const SizedBox(width: 16),
              _buildMetricItem('Jitter', '${_stats.jitterMs.toStringAsFixed(1)} ms'),
              const SizedBox(width: 16),
              _buildMetricItem('Loss', '${(_stats.packetLossRate * 100).toStringAsFixed(2)}%'),
              const SizedBox(width: 16),
              _buildMetricItem('Packets', '${_stats.packetsReceived} rx / ${_stats.packetsSent} tx'),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildMetricItem(String label, String value) {
    return Row(
      children: [
        Text('$label: ', style: const TextStyle(fontSize: 11, color: Color(0xFF8B949E))),
        Text(value, style: const TextStyle(fontSize: 11, fontWeight: FontWeight.w600, color: Color(0xFFC9D1D9))),
      ],
    );
  }

  Widget _buildTranscriptArea() {
    if (_transcripts.isEmpty) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.record_voice_over_outlined, size: 48, color: const Color(0xFF30363D)),
            const SizedBox(height: 12),
            const Text(
              'No active conversation subtitles yet.\nStart a call to stream live on-device translation.',
              textAlign: TextAlign.center,
              style: TextStyle(color: Color(0xFF8B949E), fontSize: 13, height: 1.4),
            ),
          ],
        ),
      );
    }

    return ListView.builder(
      controller: _scrollController,
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
      itemCount: _transcripts.length,
      itemBuilder: (context, index) {
        final item = _transcripts[index];
        final isLocal = item.speaker == 'local';

        return Container(
          margin: const EdgeInsets.only(bottom: 16),
          child: Row(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisAlignment: isLocal ? MainAxisAlignment.end : MainAxisAlignment.start,
            children: [
              if (!isLocal) _buildAvatar('Remote', const Color(0xFF3B82F6)),
              const SizedBox(width: 12),
              Flexible(
                child: Container(
                  padding: const EdgeInsets.all(14),
                  decoration: BoxDecoration(
                    color: isLocal ? const Color(0xFF1E293B) : const Color(0xFF161B22),
                    borderRadius: BorderRadius.circular(12),
                    border: Border.all(
                      color: isLocal ? const Color(0xFF334155) : const Color(0xFF30363D),
                    ),
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Text(
                            isLocal ? 'You' : 'Remote Peer',
                            style: TextStyle(
                              fontSize: 12,
                              fontWeight: FontWeight.bold,
                              color: isLocal ? const Color(0xFF60A5FA) : const Color(0xFF34D399),
                            ),
                          ),
                          const SizedBox(width: 8),
                          Text(
                            DateFormat('HH:mm:ss').format(item.timestamp),
                            style: const TextStyle(fontSize: 10, color: Color(0xFF6E7681)),
                          ),
                          const Spacer(),
                          Container(
                            padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
                            decoration: BoxDecoration(
                              color: const Color(0xFF0F172A),
                              borderRadius: BorderRadius.circular(4),
                            ),
                            child: Text(
                              '${item.latency.totalMs.toStringAsFixed(0)}ms',
                              style: const TextStyle(fontSize: 10, color: Color(0xFFA5B4FC)),
                            ),
                          ),
                        ],
                      ),
                      if (item.originalText.isNotEmpty) ...[
                        const SizedBox(height: 6),
                        Text(
                          item.originalText,
                          style: const TextStyle(fontSize: 13, color: Color(0xFF8B949E), fontStyle: FontStyle.italic),
                        ),
                      ],
                      const SizedBox(height: 4),
                      Text(
                        item.translatedText,
                        style: const TextStyle(
                          fontSize: 15,
                          fontWeight: FontWeight.w500,
                          color: Color(0xFFF0F6FC),
                          height: 1.3,
                        ),
                      ),
                    ],
                  ),
                ),
              ),
              const SizedBox(width: 12),
              if (isLocal) _buildAvatar('You', const Color(0xFF6366F1)),
            ],
          ),
        );
      },
    );
  }

  Widget _buildAvatar(String label, Color color) {
    return CircleAvatar(
      radius: 16,
      backgroundColor: color.withOpacity(0.2),
      child: Text(
        label[0],
        style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: color),
      ),
    );
  }

  Widget _buildBottomControlPanel() {
    final isOnline = _status == CallStatus.connected;
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 18),
      decoration: const BoxDecoration(
        color: Color(0xFF161B22),
        border: Border(top: BorderSide(color: Color(0xFF30363D))),
      ),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Row(
            children: [
              IconButton(
                icon: Icon(_isMuted ? Icons.mic_off : Icons.mic),
                color: _isMuted ? const Color(0xFFF85149) : const Color(0xFFC9D1D9),
                onPressed: () {
                  setState(() => _isMuted = !_isMuted);
                },
              ),
              IconButton(
                icon: Icon(_isDeafened ? Icons.volume_off : Icons.volume_up),
                color: _isDeafened ? const Color(0xFFF85149) : const Color(0xFFC9D1D9),
                onPressed: () {
                  setState(() => _isDeafened = !_isDeafened);
                },
              ),
            ],
          ),
          ElevatedButton.icon(
            style: ElevatedButton.styleFrom(
              backgroundColor: isOnline ? const Color(0xFFDA3633) : const Color(0xFF238636),
              padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 14),
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
            ),
            icon: Icon(isOnline ? Icons.call_end : Icons.call, color: Colors.white),
            label: Text(
              isOnline ? 'End Live Session' : 'Start Live Session',
              style: const TextStyle(color: Colors.white, fontWeight: FontWeight.bold),
            ),
            onPressed: _toggleCall,
          ),
          Row(
            children: [
              Text(
                'Peer: ${_config.remoteHost}:${_config.remotePort}',
                style: const TextStyle(fontSize: 12, color: Color(0xFF8B949E)),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

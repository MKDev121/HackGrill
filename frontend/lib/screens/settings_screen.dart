import 'package:flutter/material.dart';
import '../models/call_state.dart';
import '../services/audio_service.dart';

class SettingsScreen extends StatefulWidget {
  final CallSessionConfig config;
  final AudioService audioService;
  final ValueChanged<CallSessionConfig> onConfigChanged;

  const SettingsScreen({
    Key? key,
    required this.config,
    required this.audioService,
    required this.onConfigChanged,
  }) : super(key: key);

  @override
  State<SettingsScreen> createState() => _SettingsScreenState();
}

class _SettingsScreenState extends State<SettingsScreen> {
  late TextEditingController _hostController;
  late TextEditingController _remotePortController;
  late TextEditingController _localPortController;
  late String _sourceLang;
  late String _targetLang;
  late double _chunkMs;

  final List<Map<String, String>> _languages = [
    {'code': 'hi-IN', 'name': 'Hindi (हिन्दी)'},
    {'code': 'en-IN', 'name': 'English (Indian)'},
    {'code': 'ta-IN', 'name': 'Tamil (தமிழ்)'},
    {'code': 'te-IN', 'name': 'Telugu (తెలుగు)'},
    {'code': 'kn-IN', 'name': 'Kannada (ಕನ್ನಡ)'},
    {'code': 'mr-IN', 'name': 'Marathi (मराठी)'},
    {'code': 'bn-IN', 'name': 'Bengali (বাংলা)'},
    {'code': 'gu-IN', 'name': 'Gujarati (ગુજરાતી)'},
    {'code': 'ml-IN', 'name': 'Malayalam (മലയാളം)'},
  ];

  @override
  void initState() {
    super.initState();
    _hostController = TextEditingController(text: widget.config.remoteHost);
    _remotePortController = TextEditingController(text: widget.config.remotePort.toString());
    _localPortController = TextEditingController(text: widget.config.localPort.toString());
    _sourceLang = widget.config.sourceLanguage;
    _targetLang = widget.config.targetLanguage;
    _chunkMs = widget.config.chunkDurationMs.toDouble();
  }

  void _save() {
    final updated = CallSessionConfig(
      remoteHost: _hostController.text.trim(),
      remotePort: int.tryParse(_remotePortController.text.trim()) ?? 5006,
      localPort: int.tryParse(_localPortController.text.trim()) ?? 5004,
      sourceLanguage: _sourceLang,
      targetLanguage: _targetLang,
      chunkDurationMs: _chunkMs.toInt(),
    );
    widget.onConfigChanged(updated);
    Navigator.of(context).pop();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D1117),
      appBar: AppBar(
        backgroundColor: const Color(0xFF161B22),
        elevation: 0,
        title: const Text('Session & Engine Settings', style: TextStyle(fontSize: 16, color: Colors.white)),
        actions: [
          TextButton(
            onPressed: _save,
            child: const Text('Save', style: TextStyle(color: Color(0xFF58A6FF), fontWeight: FontWeight.bold)),
          ),
          const SizedBox(width: 8),
        ],
      ),
      body: ListView(
        padding: const EdgeInsets.all(24),
        children: [
          _buildSectionHeader('Sarvam Edge Language Configuration'),
          _buildCard([
            _buildDropdown('Source Spoken Language', _sourceLang, (val) => setState(() => _sourceLang = val!)),
            const Divider(color: Color(0xFF30363D)),
            _buildDropdown('Target Translation Language', _targetLang, (val) => setState(() => _targetLang = val!)),
            const Divider(color: Color(0xFF30363D)),
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 8),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      const Text('Audio Chunk Duration (Latency vs Quality)', style: TextStyle(fontSize: 13, color: Color(0xFFC9D1D9))),
                      Text('${_chunkMs.toInt()} ms', style: const TextStyle(fontSize: 13, fontWeight: FontWeight.bold, color: Color(0xFF58A6FF))),
                    ],
                  ),
                  Slider(
                    value: _chunkMs,
                    min: 100,
                    max: 1000,
                    divisions: 18,
                    activeColor: const Color(0xFF6366F1),
                    onChanged: (val) => setState(() => _chunkMs = val),
                  ),
                ],
              ),
            ),
          ]),
          const SizedBox(height: 24),
          _buildSectionHeader('Network & Transmission (C++ Engine)'),
          _buildCard([
            _buildTextField('Remote Peer Host', _hostController),
            const Divider(color: Color(0xFF30363D)),
            _buildTextField('Remote UDP Port', _remotePortController),
            const Divider(color: Color(0xFF30363D)),
            _buildTextField('Local UDP Port', _localPortController),
          ]),
          const SizedBox(height: 24),
          _buildSectionHeader('Hardware & Audio I/O Devices'),
          _buildCard([
            ListTile(
              leading: const Icon(Icons.mic, color: Color(0xFF58A6FF)),
              title: const Text('Microphone Capture', style: TextStyle(fontSize: 14, color: Color(0xFFC9D1D9))),
              subtitle: Text(widget.audioService.selectedInputId ?? 'Default', style: const TextStyle(fontSize: 12, color: Color(0xFF8B949E))),
            ),
            const Divider(color: Color(0xFF30363D)),
            ListTile(
              leading: const Icon(Icons.volume_up, color: Color(0xFF3FB950)),
              title: const Text('Speaker Output', style: TextStyle(fontSize: 14, color: Color(0xFFC9D1D9))),
              subtitle: Text(widget.audioService.selectedOutputId ?? 'Default', style: const TextStyle(fontSize: 12, color: Color(0xFF8B949E))),
            ),
          ]),
        ],
      ),
    );
  }

  Widget _buildSectionHeader(String title) {
    return Padding(
      padding: const EdgeInsets.only(left: 4, bottom: 8),
      child: Text(
        title.toUpperCase(),
        style: const TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Color(0xFF8B949E), letterSpacing: 0.8),
      ),
    );
  }

  Widget _buildCard(List<Widget> children) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
      decoration: BoxDecoration(
        color: const Color(0xFF161B22),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: const Color(0xFF30363D)),
      ),
      child: Column(children: children),
    );
  }

  Widget _buildDropdown(String label, String value, ValueChanged<String?> onChanged) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontSize: 13, color: Color(0xFFC9D1D9))),
          DropdownButton<String>(
            value: value,
            dropdownColor: const Color(0xFF21262D),
            underline: const SizedBox(),
            style: const TextStyle(fontSize: 13, color: Color(0xFF58A6FF)),
            items: _languages.map((l) {
              return DropdownMenuItem<String>(
                value: l['code'],
                child: Text(l['name']!),
              );
            }).toList(),
            onChanged: onChanged,
          ),
        ],
      ),
    );
  }

  Widget _buildTextField(String label, TextEditingController controller) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          Expanded(flex: 2, child: Text(label, style: const TextStyle(fontSize: 13, color: Color(0xFFC9D1D9)))),
          Expanded(
            flex: 3,
            child: TextField(
              controller: controller,
              style: const TextStyle(fontSize: 13, color: Colors.white),
              decoration: const InputDecoration(
                isDense: true,
                border: InputBorder.none,
                contentPadding: EdgeInsets.symmetric(vertical: 8),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

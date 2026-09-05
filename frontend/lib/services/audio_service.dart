import 'dart:async';
import 'package:flutter/foundation.dart';

class AudioDevice {
  final String id;
  final String name;
  final bool isInput;

  const AudioDevice({required this.id, required this.name, required this.isInput});
}

class AudioService extends ChangeNotifier {
  bool _isCapturing = false;
  bool _isPlaying = false;
  double _micLevel = 0.0;
  double _speakerLevel = 0.0;

  bool get isCapturing => _isCapturing;
  bool get isPlaying => _isPlaying;
  double get micLevel => _micLevel;
  double get speakerLevel => _speakerLevel;

  List<AudioDevice> inputDevices = [
    const AudioDevice(id: 'default_mic', name: 'Default Microphone', isInput: true),
    const AudioDevice(id: 'usb_headset_in', name: 'USB Headset Mic', isInput: true),
  ];

  List<AudioDevice> outputDevices = [
    const AudioDevice(id: 'default_spk', name: 'Default Speakers', isInput: false),
    const AudioDevice(id: 'usb_headset_out', name: 'USB Headset Audio', isInput: false),
  ];

  String? selectedInputId = 'default_mic';
  String? selectedOutputId = 'default_spk';

  Timer? _levelTimer;

  void startCapture() {
    _isCapturing = true;
    _isPlaying = true;
    _startSimulatedLevels();
    notifyListeners();
  }

  void stopCapture() {
    _isCapturing = false;
    _isPlaying = false;
    _levelTimer?.cancel();
    _micLevel = 0.0;
    _speakerLevel = 0.0;
    notifyListeners();
  }

  void _startSimulatedLevels() {
    _levelTimer?.cancel();
    _levelTimer = Timer.periodic(const Duration(milliseconds: 100), (timer) {
      if (!_isCapturing) return;
      // Simulated audio VU meter activity
      _micLevel = (_micLevel + 0.2) % 1.0;
      _speakerLevel = (_speakerLevel + 0.15) % 0.8;
      notifyListeners();
    });
  }

  @override
  void dispose() {
    _levelTimer?.cancel();
    super.dispose();
  }
}

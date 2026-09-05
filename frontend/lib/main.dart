import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/ipc_client.dart';
import 'services/audio_service.dart';
import 'screens/call_screen.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const VoipTranslationApp());
}

class VoipTranslationApp extends StatefulWidget {
  const VoipTranslationApp({Key? key}) : super(key: key);

  @override
  State<VoipTranslationApp> createState() => _VoipTranslationAppState();
}

class _VoipTranslationAppState extends State<VoipTranslationApp> {
  late final IpcClient _ipcClient;
  late final AudioService _audioService;

  @override
  void initState() {
    super.initState();
    _ipcClient = IpcClient();
    _audioService = AudioService();
  }

  @override
  void dispose() {
    _ipcClient.dispose();
    _audioService.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MultiProvider(
      providers: [
        ChangeNotifierProvider.value(value: _ipcClient),
        ChangeNotifierProvider.value(value: _audioService),
      ],
      child: MaterialApp(
        title: 'VoIP Live Translation (Sarvam Edge)',
        debugShowCheckedModeBanner: false,
        theme: ThemeData(
          brightness: Brightness.dark,
          primaryColor: const Color(0xFF6366F1),
          scaffoldBackgroundColor: const Color(0xFF0D1117),
          fontFamily: 'Segoe UI',
          colorScheme: const ColorScheme.dark(
            primary: Color(0xFF6366F1),
            secondary: Color(0xFF38BDF8),
            surface: Color(0xFF161B22),
            background: Color(0xFF0D1117),
          ),
          appBarTheme: const AppBarTheme(
            backgroundColor: Color(0xFF161B22),
            elevation: 0,
          ),
        ),
        home: CallScreen(
          ipcClient: _ipcClient,
          audioService: _audioService,
        ),
      ),
    );
  }
}

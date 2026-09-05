"""
Control Socket IPC for configuration, telemetry, and live transcription events.
"""

import socket
import struct
import json
import threading
from typing import Callable, Optional, List


class ControlSocketClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9200):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.running = False
        self.on_message_callback: Optional[Callable[[dict], None]] = None
        self._recv_thread: Optional[threading.Thread] = None

    def connect(self) -> bool:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((self.host, self.port))
            self.running = True
            self._recv_thread = threading.Thread(target=self._listen_loop, daemon=True)
            self._recv_thread.start()
            return True
        except Exception as e:
            print(f"[ControlSocketClient] Connect to {self.host}:{self.port} failed: {e}")
            return False

    def send(self, message: dict) -> bool:
        if not self.sock or not self.running:
            return False
        try:
            payload = json.dumps(message).encode("utf-8")
            length_prefix = struct.pack(">I", len(payload))
            self.sock.sendall(length_prefix + payload)
            return True
        except Exception as e:
            print(f"[ControlSocketClient] Send error: {e}")
            return False

    def _listen_loop(self):
        while self.running and self.sock:
            try:
                raw_len = self.sock.recv(4)
                if not raw_len or len(raw_len) < 4:
                    break
                (length,) = struct.unpack(">I", raw_len)
                data = bytearray()
                while len(data) < length:
                    packet = self.sock.recv(length - len(data))
                    if not packet:
                        break
                    data.extend(packet)

                if len(data) == length:
                    msg = json.loads(data.decode("utf-8"))
                    if self.on_message_callback:
                        self.on_message_callback(msg)
            except Exception as e:
                break
        self.running = False

    def close(self):
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None


class ControlSocketServer:
    def __init__(self, host: str = "127.0.0.1", port: int = 9201):
        self.host = host
        self.port = port
        self.server_sock: Optional[socket.socket] = None
        self.clients: List[socket.socket] = []
        self.lock = threading.Lock()
        self.running = False
        self.on_message_callback: Optional[Callable[[dict], None]] = None

    def start(self):
        self.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_sock.bind((self.host, self.port))
        self.server_sock.listen(5)
        self.running = True

        thread = threading.Thread(target=self._accept_loop, daemon=True)
        thread.start()
        print(f"[ControlSocketServer] Python Control Server listening on {self.host}:{self.port}")

    def broadcast(self, message: dict):
        payload = json.dumps(message).encode("utf-8")
        msg_bytes = struct.pack(">I", len(payload)) + payload

        with self.lock:
            active = []
            for c in self.clients:
                try:
                    c.sendall(msg_bytes)
                    active.append(c)
                except Exception:
                    pass
            self.clients = active

    def _accept_loop(self):
        while self.running and self.server_sock:
            try:
                client_sock, _ = self.server_sock.accept()
                with self.lock:
                    self.clients.append(client_sock)
                threading.Thread(target=self._client_handler, args=(client_sock,), daemon=True).start()
            except Exception:
                break

    def _client_handler(self, client_sock: socket.socket):
        while self.running:
            try:
                raw_len = client_sock.recv(4)
                if not raw_len or len(raw_len) < 4:
                    break
                (length,) = struct.unpack(">I", raw_len)
                data = bytearray()
                while len(data) < length:
                    packet = client_sock.recv(length - len(data))
                    if not packet:
                        break
                    data.extend(packet)

                if len(data) == length:
                    msg = json.loads(data.decode("utf-8"))
                    if self.on_message_callback:
                        self.on_message_callback(msg)
            except Exception:
                break
        with self.lock:
            if client_sock in self.clients:
                self.clients.remove(client_sock)
        client_sock.close()

    def stop(self):
        self.running = False
        if self.server_sock:
            self.server_sock.close()

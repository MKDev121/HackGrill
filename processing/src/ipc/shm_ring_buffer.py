"""
Shared Memory Ring Buffer matching the C++ layout for zero-copy audio stream IPC.
"""

import sys
import mmap
import struct
import ctypes
from typing import Optional, Tuple

SHM_MAGIC = 0x53484D52  # 'SHMR'
FRAME_MAGIC = 0x4652414D  # 'FRAM'

# Struct formats:
# RingBufferHeader:
# magic (uint32), version (uint32), capacity (uint64), write_pos (uint64),
# read_pos (uint64), sample_rate (uint32), channels (uint16), bit_depth (uint16),
# sequence (uint64), reserved (16s)
HEADER_FORMAT = "<IIQQQIHHQ16s"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)  # 64 bytes

# AudioFrameHeader:
# magic (uint32), timestamp_us (uint64), sequence_num (uint64), payload_size (uint32),
# sample_rate (uint16), channels (uint8), flags (uint8)
FRAME_HEADER_FORMAT = "<IQQIHBB"
FRAME_HEADER_SIZE = struct.calcsize(FRAME_HEADER_FORMAT)  # 28 bytes


class ShmRingBuffer:
    def __init__(self, name: str, capacity: int = 1024 * 1024, create: bool = False):
        self.name = name
        self.capacity = capacity
        self.total_size = HEADER_SIZE + capacity
        self.create = create
        self.mm: Optional[mmap.mmap] = None
        self._map_memory()

    def _map_memory(self):
        if sys.platform == "win32":
            import msvcrt
            tagname = f"Local\\{self.name}"
            # Windows mmap: fileno=-1 creates pagefile-backed mapping
            self.mm = mmap.mmap(-1, self.total_size, tagname=tagname, access=mmap.ACCESS_WRITE)
        else:
            import os
            shm_path = f"/dev/shm/{self.name}"
            flags = os.O_CREAT | os.O_RDWR if self.create else os.O_RDWR
            fd = os.open(shm_path, flags, 0o666)
            if self.create:
                os.ftruncate(fd, self.total_size)
            self.mm = mmap.mmap(fd, self.total_size, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
            os.close(fd)

        if self.create:
            self._init_header()

    def _init_header(self):
        header_bytes = struct.pack(
            HEADER_FORMAT,
            SHM_MAGIC,
            1,                     # version
            self.capacity,         # capacity
            0,                     # write_pos
            0,                     # read_pos
            16000,                 # sample_rate
            1,                     # channels
            16,                    # bit_depth
            0,                     # sequence
            b"\x00" * 16           # reserved
        )
        self.mm.seek(0)
        self.mm.write(header_bytes)

    def read_header(self) -> dict:
        self.mm.seek(0)
        data = self.mm.read(HEADER_SIZE)
        unpacked = struct.unpack(HEADER_FORMAT, data)
        return {
            "magic": unpacked[0],
            "version": unpacked[1],
            "capacity": unpacked[2],
            "write_pos": unpacked[3],
            "read_pos": unpacked[4],
            "sample_rate": unpacked[5],
            "channels": unpacked[6],
            "bit_depth": unpacked[7],
            "sequence": unpacked[8],
        }

    def available_read_bytes(self) -> int:
        h = self.read_header()
        w = h["write_pos"]
        r = h["read_pos"]
        cap = h["capacity"]
        return (w - r) if (w >= r) else (cap - (r - w))

    def write_frame(self, pcm_bytes: bytes, timestamp_us: int = 0, sequence_num: int = 0) -> bool:
        h = self.read_header()
        cap = h["capacity"]
        payload_size = len(pcm_bytes)
        total_frame_len = FRAME_HEADER_SIZE + payload_size

        avail = cap - self.available_read_bytes() - 1
        if avail < total_frame_len:
            return False  # Buffer full

        frame_header = struct.pack(
            FRAME_HEADER_FORMAT,
            FRAME_MAGIC,
            timestamp_us,
            sequence_num,
            payload_size,
            h["sample_rate"],
            h["channels"],
            0  # flags
        )

        w = h["write_pos"]
        data_to_write = frame_header + pcm_bytes
        self._write_circular(data_to_write, w, cap)

        new_w = (w + total_frame_len) % cap
        # Update write_pos (offset 16 in header)
        self.mm.seek(16)
        self.mm.write(struct.pack("<Q", new_w))
        return True

    def read_frame(self) -> Optional[Tuple[dict, bytes]]:
        if self.available_read_bytes() < FRAME_HEADER_SIZE:
            return None

        h = self.read_header()
        cap = h["capacity"]
        r = h["read_pos"]

        header_bytes = self._read_circular(r, FRAME_HEADER_SIZE, cap)
        unpacked = struct.unpack(FRAME_HEADER_FORMAT, header_bytes)
        frame_dict = {
            "magic": unpacked[0],
            "timestamp_us": unpacked[1],
            "sequence_num": unpacked[2],
            "payload_size": unpacked[3],
            "sample_rate": unpacked[4],
            "channels": unpacked[5],
            "flags": unpacked[6],
        }

        if frame_dict["magic"] != FRAME_MAGIC or frame_dict["payload_size"] > 1024 * 1024:
            # Desync, flush read_pos to write_pos
            self.mm.seek(24)
            self.mm.write(struct.pack("<Q", h["write_pos"]))
            return None

        total_frame_len = FRAME_HEADER_SIZE + frame_dict["payload_size"]
        if self.available_read_bytes() < total_frame_len:
            return None

        payload_offset = (r + FRAME_HEADER_SIZE) % cap
        payload = self._read_circular(payload_offset, frame_dict["payload_size"], cap)

        new_r = (r + total_frame_len) % cap
        # Update read_pos (offset 24 in header)
        self.mm.seek(24)
        self.mm.write(struct.pack("<Q", new_r))

        return frame_dict, payload

    def _write_circular(self, data: bytes, offset: int, cap: int):
        first_chunk = min(len(data), cap - (offset % cap))
        start_pos = HEADER_SIZE + (offset % cap)
        self.mm.seek(start_pos)
        self.mm.write(data[:first_chunk])
        if len(data) > first_chunk:
            self.mm.seek(HEADER_SIZE)
            self.mm.write(data[first_chunk:])

    def _read_circular(self, offset: int, length: int, cap: int) -> bytes:
        first_chunk = min(length, cap - (offset % cap))
        start_pos = HEADER_SIZE + (offset % cap)
        self.mm.seek(start_pos)
        part1 = self.mm.read(first_chunk)
        if length > first_chunk:
            self.mm.seek(HEADER_SIZE)
            part2 = self.mm.read(length - first_chunk)
            return part1 + part2
        return part1

    def close(self):
        if self.mm:
            self.mm.close()
            self.mm = None

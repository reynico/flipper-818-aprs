#!/usr/bin/env python3
"""Transmit APRS test packets via BladeRF using bladeRF-cli.

Generates Bell 202 AFSK (1200/2200 Hz), FM-modulates to IQ,
writes a SC16Q11 binary, and transmits via bladeRF-cli.

Usage:
    python3 aprs_bladerf_tx.py [--freq 144800000] [--gain 20] [--count 5]
"""

import argparse
import struct
import subprocess
import tempfile
import time

import numpy as np

AUDIO_RATE = 22050
RF_RATE = 1000000
FM_DEVIATION = 3500.0
BAUD = 1200
MARK_HZ = 1200
SPACE_HZ = 2200


def crc_ccitt(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0x8408
            else:
                crc >>= 1
    return crc ^ 0xFFFF


def encode_address(call, ssid, last=False):
    out = bytearray()
    call = call.upper().ljust(6)
    for c in call[:6]:
        out.append(ord(c) << 1)
    ctrl = 0x60 | ((ssid & 0x0F) << 1)
    if last:
        ctrl |= 1
    out.append(ctrl)
    return out


def build_ax25_frame(src, src_ssid, dst, dst_ssid, payload):
    frame = bytearray()
    frame += encode_address(dst, dst_ssid)
    frame += encode_address(src, src_ssid, last=True)
    frame += bytes([0x03, 0xF0])
    frame += payload.encode("ascii")
    crc = crc_ccitt(frame)
    frame += struct.pack("<H", crc)
    return frame


def bit_stuff(frame_bytes):
    bits = [0, 1, 1, 1, 1, 1, 1, 0]
    ones = 0
    for byte in frame_bytes:
        for i in range(8):
            bit = (byte >> i) & 1
            bits.append(bit)
            if bit:
                ones += 1
                if ones == 5:
                    bits.append(0)
                    ones = 0
            else:
                ones = 0
    bits += [0, 1, 1, 1, 1, 1, 1, 0]
    return bits


def generate_afsk(stuffed_bits, preamble_flags=40):
    samples_per_bit = AUDIO_RATE / BAUD
    flag = [0, 1, 1, 1, 1, 1, 1, 0]

    all_bits = []
    for _ in range(preamble_flags):
        all_bits += flag
    all_bits += stuffed_bits
    for _ in range(4):
        all_bits += flag

    phase = 0.0
    freq = MARK_HZ
    is_mark = True
    audio = []

    for bit in all_bits:
        if bit == 0:
            is_mark = not is_mark
            freq = MARK_HZ if is_mark else SPACE_HZ

        n = int(round(samples_per_bit))
        for _ in range(n):
            audio.append(np.sin(phase))
            phase += 2.0 * np.pi * freq / AUDIO_RATE
            if phase > 2.0 * np.pi:
                phase -= 2.0 * np.pi

    return np.array(audio, dtype=np.float32)


def fm_modulate(audio):
    ratio = RF_RATE / AUDIO_RATE
    n_out = int(len(audio) * ratio)
    upsampled = np.interp(
        np.linspace(0, len(audio) - 1, n_out),
        np.arange(len(audio)),
        audio,
    )
    phase = np.cumsum(upsampled) * (2.0 * np.pi * FM_DEVIATION / RF_RATE)
    iq = np.exp(1j * phase).astype(np.complex64)
    return iq * 0.7


def iq_to_sc16q11(iq):
    i = np.clip(np.real(iq) * 2047, -2048, 2047).astype(np.int16)
    q = np.clip(np.imag(iq) * 2047, -2048, 2047).astype(np.int16)
    interleaved = np.empty(len(i) * 2, dtype=np.int16)
    interleaved[0::2] = i
    interleaved[1::2] = q
    return interleaved.tobytes()


def transmit(freq_hz, gain, count, src, ssid, payload, leadin_ms):
    frame = build_ax25_frame(src, ssid, "APZFLP", 0, payload)
    stuffed = bit_stuff(frame)
    audio = generate_afsk(stuffed)
    iq = fm_modulate(audio)

    leadin = np.ones(int(RF_RATE * leadin_ms / 1000), dtype=np.complex64) * 0.7
    silence = np.zeros(RF_RATE // 4, dtype=np.complex64)
    full_iq = np.concatenate([leadin, iq, silence])

    repeated = np.tile(full_iq, count)

    with tempfile.NamedTemporaryFile(suffix=".bin", delete=False) as f:
        f.write(iq_to_sc16q11(repeated))
        binpath = f.name

    n_samples = len(repeated)
    print(f"TX {freq_hz / 1e6:.4f} MHz, gain {gain}, {count} packets")
    print(f"Source: {src}-{ssid}")
    print(f"Payload: {payload}")
    print(f"Samples: {n_samples}, file: {binpath}")

    script = (
        f"set frequency tx {freq_hz}\n"
        f"set samplerate tx {RF_RATE}\n"
        f"set bandwidth tx 200000\n"
        f"set gain tx {gain}\n"
        f"tx config file={binpath} format=bin repeat=1\n"
        f"tx start\n"
        f"tx wait\n"
    )

    with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as sf:
        sf.write(script)
        scriptpath = sf.name

    print(f"Running bladeRF-cli...")
    result = subprocess.run(
        ["bladeRF-cli", "-s", scriptpath],
        capture_output=True,
        text=True,
    )

    if result.stdout:
        print(result.stdout)
    if result.returncode != 0:
        print(f"Error: {result.stderr}")
    else:
        print("done")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="APRS BladeRF transmitter")
    parser.add_argument("--freq", type=int, default=144800000)
    parser.add_argument("--gain", type=int, default=20)
    parser.add_argument("--count", type=int, default=5)
    parser.add_argument("--call", default="TEST01")
    parser.add_argument("--ssid", type=int, default=1)
    parser.add_argument("--payload", default=">BladeRF APRS test")
    parser.add_argument("--leadin", type=int, default=200)
    args = parser.parse_args()

    transmit(args.freq, args.gain, args.count, args.call, args.ssid, args.payload, args.leadin)

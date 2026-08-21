#!/usr/bin/env python3
"""
Minimal /voice echo server for bench-testing JLS-1 hardware + firmware, independent
of the real STT -> Claude -> TTS pipeline (which doesn't exist yet — see
../../docs/lab_audio_satellite_design.md's "LAB-server contract" section).

What it does: buffers whatever mic audio JLS-1 streams while the button is held,
and on end-of-utterance, plays it straight back as the "TTS" reply. If you can
press the button, talk, let go, and hear yourself a beat later, the whole hardware
+ firmware + network path is proven — the only thing left to build is the real
STT/Claude/TTS logic in place of this echo. This is bring-up plan item 6 from the
design doc, made runnable.

Usage:
    pip install websockets
    python3 echo_voice_server.py [--host 0.0.0.0] [--port 5081] [--token TOKEN]

Point JLS-1's include/config.h at this machine's LAN IP and the same --token (or
leave both the flag and LAB_AUTH_TOKEN blank to skip auth entirely for a first
bring-up pass — tighten it back up before this ever runs unattended).

Tested against the `websockets` package's current asyncio server API (v13+, where
websockets.serve() hands the handler a ServerConnection with a `.request` object —
this was smoke-tested end-to-end against websockets 17.0.1). If your installed
version predates that (the old `websockets.legacy` server, pre-v13), the handler
gets a connection object with top-level `.path` / `.request_headers` instead —
swap the two lines below accordingly.
"""
import argparse
import asyncio
import os
import sys

import websockets

TAG_MIC_AUDIO = 0x01
TAG_TTS_AUDIO = 0x02
TAG_END_OF_UTTERANCE = 0x03

CHUNK_BYTES = 512  # bytes of PCM per outgoing "TTS" frame, kept small like real TTS streaming would be


async def handle_connection(ws, expected_token):
    path = ws.request.path if hasattr(ws, "request") else getattr(ws, "path", "/voice")
    if path != "/voice":
        await ws.close(code=1008, reason="unexpected path")
        return

    if expected_token:
        headers = ws.request.headers if hasattr(ws, "request") else ws.request_headers
        auth = headers.get("Authorization", "")
        if auth != f"Bearer {expected_token}":
            print("[echo] rejecting connection: bad/missing Authorization header")
            await ws.close(code=1008, reason="unauthorized")
            return

    print(f"[echo] client connected from {ws.remote_address}")
    buffer = bytearray()

    try:
        async for message in ws:
            if not isinstance(message, (bytes, bytearray)) or len(message) < 1:
                continue
            tag, payload = message[0], message[1:]

            if tag == TAG_MIC_AUDIO:
                buffer.extend(payload)
            elif tag == TAG_END_OF_UTTERANCE:
                print(f"[echo] end-of-utterance, {len(buffer)} bytes captured — echoing back")
                for i in range(0, len(buffer), CHUNK_BYTES):
                    chunk = buffer[i:i + CHUNK_BYTES]
                    await ws.send(bytes([TAG_TTS_AUDIO]) + bytes(chunk))
                await ws.send(bytes([TAG_END_OF_UTTERANCE]))
                buffer.clear()
            else:
                print(f"[echo] unknown tag 0x{tag:02x}, ignoring")
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        print("[echo] client disconnected")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=5081)
    parser.add_argument("--token", default=os.environ.get("LAB_AUTH_TOKEN", ""))
    args = parser.parse_args()

    async def handler(ws):
        await handle_connection(ws, args.token)

    async def run():
        async with websockets.serve(handler, args.host, args.port):
            auth_note = " (auth required)" if args.token else " (no auth — bench testing only)"
            print(f"[echo] listening on ws://{args.host}:{args.port}/voice{auth_note}")
            await asyncio.Future()  # run forever

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        sys.exit(0)


if __name__ == "__main__":
    main()

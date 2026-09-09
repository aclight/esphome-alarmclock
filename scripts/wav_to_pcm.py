#!/usr/bin/env python3
"""Convert a PCM WAV file to a C++ header for the alarm clock sample catalog."""

import argparse
import struct
import wave


def convert(input_path: str, output_path: str, symbol: str) -> None:
    with wave.open(input_path, "rb") as source:
        if source.getcomptype() != "NONE" or source.getsampwidth() != 2:
            raise ValueError("input must be uncompressed 16-bit PCM")
        source_rate = source.getframerate()
        channel_count = source.getnchannels()
        frame_count = source.getnframes()
        samples = struct.unpack(
            f"<{frame_count * channel_count}h", source.readframes(frame_count)
        )

    mono = [
        sum(samples[frame * channel_count : (frame + 1) * channel_count])
        // channel_count
        for frame in range(frame_count)
    ]
    output_count = round(frame_count * 16000 / source_rate)
    pcm_values = []
    for output_index in range(output_count):
        source_position = output_index * source_rate / 16000
        lower = min(int(source_position), frame_count - 1)
        upper = min(lower + 1, frame_count - 1)
        fraction = source_position - lower
        value = round(mono[lower] + (mono[upper] - mono[lower]) * fraction)
        pcm_values.append(max(-32768, min(32767, value)))
    pcm = struct.pack(f"<{len(pcm_values)}h", *pcm_values)

    with open(output_path, "w", encoding="ascii", newline="\n") as header:
        header.write("#ifndef RTHAWK_PCM_H\n#define RTHAWK_PCM_H\n\n")
        header.write("#include <cstdint>\n\n")
        header.write(f"static constexpr uint8_t {symbol}[] = {{\n")
        for offset in range(0, len(pcm), 16):
            values = ", ".join(f"0x{value:02x}" for value in pcm[offset : offset + 16])
            header.write(f"    {values},\n")
        header.write("};\n\n#endif  // RTHAWK_PCM_H\n")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="source WAV")
    parser.add_argument("output", help="output C++ header")
    parser.add_argument("--symbol", default="kRedTailedHawkPcm")
    args = parser.parse_args()
    convert(args.input, args.output, args.symbol)


if __name__ == "__main__":
    main()
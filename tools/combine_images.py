#!/usr/bin/env python3

import argparse
import os
import sys


BOOTLOADER_BYTES = 0x2000
APP_MAX_BYTES = 0xE000
FLASH_FILL = 0xFF


def read_binary(path: str) -> bytes:
    with open(path, "rb") as f:
        return f.read()


def main() -> int:
    parser = argparse.ArgumentParser(description="Combine Iris bootloader and app binaries.")
    parser.add_argument("--bootloader", required=True, help="Bootloader binary path")
    parser.add_argument("--app", required=True, help="Application binary path")
    parser.add_argument("--output", required=True, help="Combined binary output path")
    args = parser.parse_args()

    bootloader = read_binary(args.bootloader)
    app = read_binary(args.app)

    if len(bootloader) > BOOTLOADER_BYTES:
        print(
            f"error: bootloader image is {len(bootloader)} bytes, exceeds reserved {BOOTLOADER_BYTES} bytes",
            file=sys.stderr,
        )
        return 1

    if len(app) > APP_MAX_BYTES:
        print(
            f"error: app image is {len(app)} bytes, exceeds reserved {APP_MAX_BYTES} bytes",
            file=sys.stderr,
        )
        return 1

    image = bytearray([FLASH_FILL] * BOOTLOADER_BYTES)
    image[: len(bootloader)] = bootloader
    image.extend(app)

    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    with open(args.output, "wb") as f:
        f.write(image)

    print(
        f"combined image written: {args.output} "
        f"(bootloader={len(bootloader)} bytes, app={len(app)} bytes, total={len(image)} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

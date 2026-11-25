"""Build a 1 MiB M-Flasher image from the pinned public FM-1 recovery base."""
import argparse
import hashlib
import struct
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / "base/tools"))
import fm1pkg_make as pkg

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("base", type=Path)
parser.add_argument("app", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
base = args.base.read_bytes()
if hashlib.sha256(base).hexdigest() != "955ee6bbb75842288b7aba18875ffd785cba1d5a899c6ee907525159340e5977":
    sys.exit("Expected the public FM1-BassStation 1.0 clean-install recovery image")
config = bytearray(base[64:96])
pkg.enc(config, 0, len(config))
off, size = struct.unpack_from("<II", config, 4)
assert config[16:30] == b"isd_config.ini" and pkg.crc16(config[2:]) == int.from_bytes(config[:2],"little")
assert pkg.crc16(base[off:off+size]) == int.from_bytes(config[2:4],"little")
key = pkg.chipkey_decode(base[off:off+32])
pkg.SDK = root / "build-deps/sdk"
app = args.app.read_bytes()
assert app.startswith(bytes.fromhex("04818000"))
built = pkg.flash_image(app, key)
wave = (root / "assets/nature/animal-wave.bin").read_bytes()
assert len(base) == 0x100000 and len(built) == 0x93000 and len(wave) == 0xb3000-0x97000
image = bytearray(base)
image[0x4000:0x93000] = built[0x4000:]
image[0x97000:0xb3000] = wave
plain = bytearray(image[0x4000:0x93000])
pkg.sfc(plain,0,len(plain),0,key)
assert plain[0x120:0x120+pkg.APP_SLOT] == app.ljust(pkg.APP_SLOT,b"\xff")
assert image[:0x4000] == base[:0x4000] and image[0x93000:0x97000] == base[0x93000:0x97000]
assert image[0x97000:0xb3000] == wave and image[0xb3000:] == base[0xb3000:]
args.output.write_bytes(image)

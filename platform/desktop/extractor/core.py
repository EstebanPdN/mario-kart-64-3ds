"""Local ROM-to-O2R conversion. This module needs only the standard library."""
from __future__ import annotations

from collections import deque
import errno
import hashlib
import os
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import zipfile

ROM_SHA1 = '579c48e211ae952530ffc8738709f078d5dd215e'
ROM_SIZE = 12 * 1024 * 1024
CHUNK = 64 * 1024
MIN_ENTRIES = 32445
REQUIRED = {
    'textures/common_data/common_texture_item_box_question_mark': b'XETO',
    'models/ceremony_data/ceremony_data_seg11_vtx_340': b'XTVO',
    'models/ceremony_data/silver_trophy_dl': b'TLDO',
    'models/startup_logo/dl1': b'TLDO',
    'sound/samples/sample_0': b'CFUA',
    'sound/banks/bank_0': b'KNAB',
    'sound/sequences/sound_player': b'CQES',
}
# The type constants are four-character integers serialized little-endian.


class Cancelled(Exception):
    pass


def check_cancel(cancel):
    if cancel is not None and cancel.is_set():
        raise Cancelled('Cancelled safely.')


def normalize_rom(source, target, progress=lambda *_: None, cancel=None):
    """Verify the complete normalized USA ROM, accepting all three byte orders."""
    sha1 = hashlib.sha1()
    sha256 = hashlib.sha256()
    with Path(source).open('rb') as input_file, Path(target).open('xb') as output:
        if os.fstat(input_file.fileno()).st_size != ROM_SIZE:
            raise ValueError('Choose an unmodified 12 MiB USA Mario Kart 64 ROM.')
        magic = input_file.read(4)
        input_file.seek(0)
        order = {b'\x80\x37\x12\x40': 1, b'\x37\x80\x40\x12': 2,
                 b'\x40\x12\x37\x80': 4}.get(magic)
        if order is None:
            raise ValueError('This file is not a recognized Nintendo 64 ROM.')
        done = 0
        while chunk := input_file.read(CHUNK):
            check_cancel(cancel)
            if order != 1:
                swapped = bytearray(len(chunk))
                for i in range(order):
                    swapped[i::order] = chunk[order-1-i::order]
                chunk = swapped
            sha1.update(chunk)
            sha256.update(chunk)
            output.write(chunk)
            done += len(chunk)
            progress('Checking ROM', int(done * 15 / ROM_SIZE))
    if done != ROM_SIZE or sha1.hexdigest() != ROM_SHA1:
        raise ValueError('Unsupported ROM. An unmodified USA Mario Kart 64 ROM is required; Japanese, European and modified ROMs are not supported.')
    check_cancel(cancel)
    return sha256.hexdigest()


def verify_archive(path, progress=lambda *_: None, cancel=None):
    """Use the console reader's identity/type checks and read every ZIP CRC."""
    with zipfile.ZipFile(path) as archive:
        members = archive.infolist()
        names = {item.filename for item in members}
        if len(members) < MIN_ENTRIES or len(names) != len(members):
            raise ValueError('The generated archive is incomplete or has duplicate entries.')
        version = archive.read('version')
        if len(version) != 5 or version[0] not in (0, 1):
            raise ValueError('The archive has an invalid game version.')
        if int.from_bytes(version[1:], 'big' if version[0] else 'little') != 0xB655503E:
            raise ValueError('The archive is for a different game revision.')
        for name, kind in REQUIRED.items():
            with archive.open(name) as item:
                header = item.read(65)
            if len(header) <= 64 or header[0] != 0 or header[4:8] != kind or header[8:12] != bytes(4):
                raise ValueError('Invalid game resource: ' + name)
        total = sum(item.file_size for item in members)
        done = 0
        for info in members:
            check_cancel(cancel)
            with archive.open(info) as item:
                while data := item.read(CHUNK):
                    check_cancel(cancel)
                    done += len(data)
                    progress('Verifying game data', 80 + min(19, int(done * 19 / max(1, total))))
    return len(members)


def run_extractor(binary, workspace, yaml_count, progress, cancel):
    """Keep the UI cancellable even when Torch is inside a long factory call."""
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    process = subprocess.Popen([str(binary), 'input.z64', 'metadata', 'output'],
        cwd=workspace, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        creationflags=flags)
    messages = queue.Queue(maxsize=256)
    stop = threading.Event()

    def read_lines():
        try:
            for line in iter(process.stdout.readline, b''):
                while not stop.is_set():
                    try:
                        messages.put(line.decode('utf-8', 'replace')[:2048], timeout=.1)
                        break
                    except queue.Full:
                        pass
        finally:
            process.stdout.close()

    reader = threading.Thread(target=read_lines, daemon=True)
    reader.start()
    recent = deque(maxlen=12)
    completed = 0
    try:
        while process.poll() is None or reader.is_alive() or not messages.empty():
            check_cancel(cancel)
            try:
                line = messages.get(timeout=.08)
            except queue.Empty:
                continue
            if line.startswith('MK64_PROGRESS\t'):
                completed += 1
                progress('Preparing your game', 15 + min(64, int(64 * completed / max(1, yaml_count))))
            else:
                recent.append(line.strip())
        if process.wait() != 0:
            raise RuntimeError('The extractor could not finish.\n' + '\n'.join(recent))
    finally:
        stop.set()
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        reader.join(timeout=3)


def convert(source, output, progress=lambda *_: None, cancel=None, *, replace=False,
            resources=None):
    source = Path(source).expanduser().resolve(strict=True)
    output = Path(output).expanduser().absolute()
    if source == output.resolve() or (output.exists() and os.path.samefile(source, output)):
        raise ValueError('The input ROM cannot be used as the output file.')
    if output.suffix.lower() != '.o2r':
        raise ValueError('Save game data with the .o2r extension.')
    if output.is_symlink():
        raise ValueError('Choose a regular output file, rather than a symbolic link.')
    if output.exists() and not replace:
        raise FileExistsError('The output already exists. Choose another filename.')
    if not output.parent.is_dir():
        raise ValueError('The selected output folder does not exist.')
    if shutil.disk_usage(output.parent).free < 256 * 1024 * 1024:
        raise ValueError('At least 256 MiB of free space is needed in the output folder.')
    root = Path(resources) if resources else Path(__file__).resolve().parent / 'runtime'
    binary = (root / 'bin' / ('mk64-extract.exe' if os.name == 'nt' else 'mk64-extract')).resolve()
    metadata = root / 'metadata'
    if not binary.is_file() or not (metadata / 'config.yml').is_file():
        raise RuntimeError('The extraction engine is missing. Run build.py before launching the source version, or use the complete application package.')
    progress('Checking ROM', 0)
    # Sibling staging guarantees the final rename stays on one filesystem.
    # Only this uniquely owned folder is deleted on failure or cancellation.
    with tempfile.TemporaryDirectory(prefix='.mk64-extract-', dir=output.parent) as directory:
        workspace = Path(directory)
        rom_hash = normalize_rom(source, workspace / 'input.z64', progress, cancel)
        check_cancel(cancel)
        shutil.copytree(metadata, workspace / 'metadata')
        yaml_count = sum(1 for p in (metadata / 'yamls').rglob('*') if p.suffix in ('.yml', '.yaml'))
        run_extractor(binary, workspace, yaml_count, progress, cancel)
        generated = workspace / 'output' / 'mk64.o2r'
        entries = verify_archive(generated, progress, cancel)
        check_cancel(cancel)
        # Flush before committing a fully verified output.
        with generated.open('r+b') as data:
            os.fsync(data.fileno())
        if not replace and output.exists():
            raise FileExistsError('The output appeared during extraction. It was preserved.')
        if replace:
            os.replace(generated, output)
        else:
            # Atomic no-clobber publication on filesystems that support links.
            # The fallback exclusively reserves the name on FAT/exFAT SD cards.
            try:
                os.link(generated, output)
            except OSError as error:
                if error.errno not in (errno.EPERM, errno.ENOTSUP, errno.EOPNOTSUPP,
                                      errno.EXDEV, errno.EINVAL, errno.ENOSYS) and getattr(error, 'winerror', None) not in (1, 50):
                    raise
                fd = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                os.close(fd)
                try:
                    os.replace(generated, output)
                except BaseException:
                    output.unlink(missing_ok=True)
                    raise
        progress('Ready!', 100)
        return {'path': str(output), 'entries': entries, 'size': output.stat().st_size,
                'rom_sha256': rom_hash}

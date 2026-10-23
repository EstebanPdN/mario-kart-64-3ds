import hashlib
import errno
import os
from pathlib import Path
import struct
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile
import shutil

import core


class ExtractionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def test_three_rom_byte_orders_normalize_to_the_same_bytes(self):
        original = b'\x80\x37\x12\x40' + bytes(range(256))*4
        for order in (1, 2, 4):
            swapped = bytearray(len(original))
            for i in range(order):
                swapped[i::order] = original[order-1-i::order]
            source, output = self.root/f'{order}.rom', self.root/f'{order}.z64'
            source.write_bytes(swapped)
            with patch.object(core, 'ROM_SIZE', len(original)), patch.object(core, 'ROM_SHA1', hashlib.sha1(original).hexdigest()):
                core.normalize_rom(source, output)
            self.assertEqual(output.read_bytes(), original)
            self.assertEqual(source.read_bytes(), bytes(swapped))

    def test_renaming_wrong_rom_does_not_bypass_full_hash(self):
        source = self.root/'USA.z64'
        source.write_bytes(b'\x80\x37\x12\x40' + bytes(1024))
        with patch.object(core, 'ROM_SIZE', 1028), self.assertRaisesRegex(ValueError, 'Unsupported ROM'):
            core.normalize_rom(source, self.root/'normalized.z64')

    def test_existing_output_and_input_cannot_be_replaced_by_default(self):
        source, output = self.root/'input.z64', self.root/'mk64.o2r'
        source.write_bytes(b'original ROM'); output.write_bytes(b'existing game data')
        with self.assertRaises(FileExistsError):
            core.convert(source, output)
        with self.assertRaisesRegex(ValueError, 'input ROM'):
            core.convert(source, source, replace=True)
        self.assertEqual(output.read_bytes(), b'existing game data')
        self.assertEqual(source.read_bytes(), b'original ROM')

    def test_failure_preserves_existing_output_and_cleans_only_own_work(self):
        source, output = self.root/'input.z64', self.root/'mk64.o2r'
        source.write_bytes(b'bad ROM'); output.write_bytes(b'existing game data')
        engine = self.root/'runtime/bin/mk64-extract'
        engine.parent.mkdir(parents=True); engine.touch()
        metadata = self.root/'runtime/metadata'; metadata.mkdir(); (metadata/'config.yml').touch()
        with self.assertRaises(ValueError):
            core.convert(source, output, replace=True, resources=self.root/'runtime')
        self.assertEqual(output.read_bytes(), b'existing game data')
        self.assertFalse(list(self.root.glob('.mk64-extract-*')))

    def test_cancelled_normalization_stops_before_extraction(self):
        source = self.root/'input.z64'
        original = b'\x80\x37\x12\x40' + bytes(1024)
        source.write_bytes(original)
        cancel = threading.Event(); cancel.set()
        with patch.object(core, 'ROM_SIZE', len(original)), self.assertRaises(core.Cancelled):
            core.normalize_rom(source, self.root/'out.z64', cancel=cancel)
        self.assertEqual(source.read_bytes(), original)

    def fixture(self, version=0xB655503E):
        output = self.root/'fixture.o2r'
        with zipfile.ZipFile(output, 'w') as z:
            z.writestr('version', b'\x01' + version.to_bytes(4, 'big'))
            for name, kind in core.REQUIRED.items():
                z.writestr(name, bytes(4)+kind+bytes(56)+b'payload')
        return output

    def test_archive_identity_and_complete_readback(self):
        with patch.object(core, 'MIN_ENTRIES', 8):
            self.assertEqual(core.verify_archive(self.fixture()), 8)

    def test_different_game_version_is_rejected(self):
        with patch.object(core, 'MIN_ENTRIES', 8), self.assertRaisesRegex(ValueError, 'different game revision'):
            core.verify_archive(self.fixture(0x12345678))

    def test_payload_corruption_is_caught_by_readback_crc(self):
        output = self.fixture()
        data = bytearray(output.read_bytes())
        offset = data.index(b'payload')
        data[offset] ^= 0xFF
        output.write_bytes(data)
        with patch.object(core, 'MIN_ENTRIES', 8), self.assertRaises(zipfile.BadZipFile):
            core.verify_archive(output)

    def test_incomplete_archive_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            core.verify_archive(self.fixture())

    def test_sd_filesystem_without_hardlinks_can_save_verified_output(self):
        fixture=self.fixture()
        source=self.root/'input.z64';source.touch()
        engine=self.root/'runtime/bin/mk64-extract';engine.parent.mkdir(parents=True);engine.touch()
        metadata=self.root/'runtime/metadata';metadata.mkdir();(metadata/'config.yml').touch()
        (metadata/'yamls').mkdir()
        def run(binary,workspace,*args):
            (workspace/'output').mkdir()
            shutil.copy2(fixture,workspace/'output/mk64.o2r')
        output=self.root/'mk64.o2r'
        with patch.object(core,'normalize_rom',return_value='fixture'), patch.object(core,'run_extractor',side_effect=run), patch.object(core,'MIN_ENTRIES',8), patch.object(core.os,'link',side_effect=OSError(errno.EINVAL,'Links unsupported')):
            core.convert(source,output,resources=self.root/'runtime')
        self.assertEqual(output.read_bytes(),fixture.read_bytes())
        self.assertFalse(list(self.root.glob('.mk64-extract-*')))

    @unittest.skipIf(os.name == 'nt', 'POSIX test fixture; native Windows worker is an EXE')
    def test_silent_long_running_worker_can_be_cancelled(self):
        runner = self.root/'worker'
        runner.write_text('#!/bin/sh\nexec /bin/sleep 20\n')
        runner.chmod(0o755)
        cancel = threading.Event()
        timer = threading.Timer(.15, cancel.set); timer.start()
        start = time.monotonic()
        with self.assertRaises(core.Cancelled):
            core.run_extractor(runner, self.root, 1, lambda *_: None, cancel)
        timer.join()
        self.assertLess(time.monotonic()-start, 4)


if __name__ == '__main__':
    unittest.main()

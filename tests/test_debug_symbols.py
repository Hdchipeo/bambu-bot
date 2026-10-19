import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import hashlib

spec=importlib.util.spec_from_file_location('archive_debug_symbols',Path(__file__).resolve().parents[1]/'scripts/archive_debug_symbols.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class DebugSymbolTests(unittest.TestCase):
    def test_matching_sha_dedup_and_retention(self):
        with tempfile.TemporaryDirectory() as tmp:
            build=Path(tmp);(build/'project_description.json').write_text(json.dumps({'project_version':'diagnostic-test'}))
            directory=build/'diagnostics';directory.mkdir()
            sidecar=directory/('._'+'a'*64+'.elf');sidecar.write_bytes(b'AppleDouble')
            sidecar.with_suffix('.json').write_bytes(b'\x00\xffAppleDouble')
            previous=None;history=[]
            for i in range(5):
                data=b'ELF-fixture'+bytes([i]);(build/'bambu_bot.elf').write_bytes(data)
                path=module.archive(build)
                self.assertEqual(path.stem,hashlib.sha256(data).hexdigest());self.assertEqual(path.read_bytes(),data)
                self.assertEqual(module.archive(build),path)
                actual=[p for p in directory.glob('*.elf') if not p.name.startswith('._')]
                self.assertLessEqual(len(actual),3)
                self.assertTrue(json.loads(path.with_suffix('.json').read_text())['private_symbols'])
                previous=path;history.append(path.name)
                self.assertEqual({p.name for p in actual},set(history[-3:]))
            self.assertTrue(previous.exists())
            self.assertEqual(sidecar.read_bytes(),b'AppleDouble')

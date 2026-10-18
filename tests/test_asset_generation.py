import importlib.util
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('build_default_assets',Path(__file__).resolve().parents[1]/'scripts/build_default_assets.py')
builder=importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class AssetGenerationTests(unittest.TestCase):
    def test_only_real_images_enter_index_including_nested_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp)/'images';output=Path(tmp)/'output'
            source.mkdir();output.mkdir()
            (source/'happy.png').write_bytes(b'actual-image-placeholder')
            (source/'._happy.png').write_bytes(b'AppleDouble')
            (source/'.hidden').mkdir();(source/'.hidden'/'sad.png').write_bytes(b'metadata')
            entries=builder.process_emoji_collection(str(source),str(output))
            self.assertEqual(entries,[{'name':'happy','file':'happy.png'}])
            self.assertEqual({p.name for p in output.iterdir() if not p.name.startswith('.')},{'happy.png'})

    def test_otto_aliases_remain_for_real_images_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp)/'otto-gif';output=Path(tmp)/'output'
            source.mkdir();output.mkdir()
            (source/'happy.gif').write_bytes(b'gif-placeholder')
            (source/'._happy.gif').write_bytes(b'AppleDouble')
            entries=builder.process_emoji_collection(str(source),str(output))
            self.assertIn({'name':'delicious','file':'happy.gif'},entries)
            self.assertTrue(all(not e['name'].startswith('.') and e['file']=='happy.gif' for e in entries))

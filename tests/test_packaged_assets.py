import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "packaged_assets", Path(__file__).resolve().parents[1] / "scripts/check-packaged-assets.py"
)
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class PackagedAssetsTest(unittest.TestCase):
    def test_directory_prefixes_and_exact_filenames(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "res/components").mkdir(parents=True)
            (root / "res/components/jack.svg").write_text("<svg/>")
            target = {"root": root, "label": "fixture"}
            with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(assets.validate_one(target, {"res/components/": ["fixture.cpp"]}), 0)
                self.assertEqual(assets.validate_one(target, {"res/components/jack.svg": ["fixture.cpp"]}), 0)
                self.assertEqual(assets.validate_one(target, {"res/missing/": ["fixture.cpp"]}), 1)
                self.assertEqual(assets.validate_one(target, {"res/component/": ["fixture.cpp"]}), 1)
                self.assertEqual(assets.validate_one(target, {"res/components/missing.svg": ["fixture.cpp"]}), 1)
                (root / "res/empty").mkdir()
                self.assertEqual(assets.validate_one(target, {"res/empty/": ["fixture.cpp"]}), 1)


if __name__ == "__main__":
    unittest.main()

"""Adversarial packaging checks; fixtures do not count as GPU evidence."""
import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "holomorphic_verify_packaged_fragment",
    ROOT / "_" / "shader" / "verify-packaged-fragment.py",
)
MOD = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MOD)

SOURCE = b"#version 300 es\nprecision highp float;\nvoid main(){}\n"
SHA = hashlib.sha256(SOURCE).hexdigest()
RECEIPT = (
    "compiler=idris2-glsles\n"
    "backend_repo=fuego-ironworks/idris-shader-backend\n"
    "application_repo=isomorphismes/holomorphic\n"
    "backend_commit=" + "a" * 40 + "\n"
    "application_commit=" + "b" * 40 + "\n"
    "fragment_sha256=" + SHA + "\n"
).encode()


class PackagedFragmentTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.shader = self.root / "continuation.frag"
        self.shader.write_bytes(SOURCE)
        self.receipt = self.root / "continuation.provenance"
        self.receipt.write_bytes(RECEIPT)

    def make_package(self, extension=".apk", changed_source=None, changed_receipt=None,
                     extra_shader=False, drop_receipt=False):
        prefix = "assets/" if extension == ".apk" else "base/assets/"
        output = self.root / ("application" + extension)
        with ZipFile(output, "w", ZIP_DEFLATED) as archive:
            archive.writestr(prefix + "continuation.frag",
                             SOURCE if changed_source is None else changed_source)
            if extra_shader:
                archive.writestr(prefix + "continuation.frag", SOURCE)
            if not drop_receipt:
                archive.writestr(prefix + "continuation.provenance",
                                 RECEIPT if changed_receipt is None else changed_receipt)
        return output

    def test_exact_apk_and_aab(self):
        for extension in (".apk", ".aab"):
            with self.subTest(extension=extension):
                MOD.verify_package(self.make_package(extension), self.shader, self.receipt)

    def test_reject_swapped_shader_asset(self):
        with self.assertRaisesRegex(ValueError, "bytes mismatch"):
            MOD.verify_package(self.make_package(changed_source=b"void main(){}"),
                               self.shader, self.receipt)

    def test_reject_missing_receipt_asset(self):
        with self.assertRaisesRegex(ValueError, "missing/duplicate"):
            MOD.verify_package(self.make_package(drop_receipt=True), self.shader, self.receipt)

    def test_reject_changed_receipt_asset(self):
        with self.assertRaisesRegex(ValueError, "bytes mismatch"):
            MOD.verify_package(self.make_package(changed_receipt=b"compiler=handwritten\n"),
                               self.shader, self.receipt)

    def test_reject_duplicate_shader_asset(self):
        with self.assertRaisesRegex(ValueError, "missing/duplicate"):
            MOD.verify_package(self.make_package(extra_shader=True), self.shader, self.receipt)

    def test_reject_wrong_receipt_hash(self):
        self.receipt.write_bytes(RECEIPT.replace(SHA.encode(), b"0" * 64))
        with self.assertRaisesRegex(ValueError, "fragment_sha256"):
            MOD.verify_package(self.make_package(), self.shader, self.receipt)

    def test_reject_forged_compiler_label(self):
        self.receipt.write_bytes(RECEIPT.replace(b"idris2-glsles", b"handwritten"))
        with self.assertRaisesRegex(ValueError, "compiler"):
            MOD.verify_package(self.make_package(), self.shader, self.receipt)


if __name__ == "__main__":
    unittest.main()

import os
import unittest
from collections import UserDict, UserList

import sxpb


def without_terminal_lf(text):
    return text[:-1] if text.endswith("\n") else text


def type_snapshot(value):
    if isinstance(value, UserDict):
        return (
            type(value).__name__,
            tuple((key, type_snapshot(item)) for key, item in value.items()),
        )
    if isinstance(value, UserList):
        return (type(value).__name__, tuple(type_snapshot(item) for item in value))
    return (type(value).__name__, value)


class TestSxpbFiles(unittest.TestCase):
    def test_validate_sxpb_files(self):
        dirs_file = os.path.join(os.path.dirname(__file__), "dirs.sxpb")
        dirs_to_scan = sxpb.load(dirs_file)

        sxpb_files = []
        corpus_counts = {"golden": 0, "idempotent": 0}
        # If we are in root, dirs are relative to root.
        # But we should be careful about where the test is run from.
        # Assuming run from root as per `pdm test`.

        for d in dirs_to_scan:
            if not os.path.exists(d):
                continue
            for root, _, files in os.walk(d):
                for file in files:
                    if file.endswith(".sxpb"):
                        sxpb_files.append(os.path.join(root, file))
                        corpus_name = os.path.basename(root)
                        if corpus_name in corpus_counts:
                            corpus_counts[corpus_name] += 1

        self.assertTrue(len(sxpb_files) > 0, "No .sxpb files found")
        for corpus_name, count in corpus_counts.items():
            self.assertGreater(count, 0, f"No {corpus_name} corpus files found")

        for sxpb_file in sxpb_files:
            with self.subTest(sxpb_file=sxpb_file):
                # 1. Read the file via the library, using precise=True
                obj1 = sxpb.load(sxpb_file, precise=True)

                # 2. Write the sxpb to a string
                s1 = sxpb.dumps(obj1)

                if os.path.basename(os.path.dirname(sxpb_file)) == "idempotent":
                    with open(
                        sxpb_file, encoding="utf-8", newline=""
                    ) as source_file:
                        source = source_file.read()
                    self.assertEqual(
                        without_terminal_lf(source),
                        without_terminal_lf(s1),
                        f"Canonical print changed {sxpb_file}",
                    )

                # 3. Parse sxpb from the string, using precise=True
                obj2 = sxpb.loads(s1, precise=True)

                # 4. Ensure parse-print-parse preserves precise container/scalar types.
                self.assertEqual(
                    type_snapshot(obj1),
                    type_snapshot(obj2),
                    f"Semantic roundtrip failed for {sxpb_file}",
                )


if __name__ == "__main__":
    unittest.main()

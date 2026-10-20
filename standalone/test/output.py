"""Exercise installed-style CLI use from outside the source directory."""
import csv
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="paramagnetics-cli-") as folder:
    root = Path(folder)
    scene = {
        "magnets": [{"id": 16777217, "type": "cuboid", "parameters": {
            "dimensions": [0.01, 0.01, 0.01], "magnetization": [0, 0, 1],
            "position": [0, 0, 0], "orientation": [1, 0, 0, 0]}}],
        "field_of_view": {"quantity": "H", **{
            axis: {"min": 0.1, "max": 0.2, "n": 2} for axis in "xyz"}},
        "force": {"targets": "all"},
    }
    source = root / "scene.json"
    source.write_text(json.dumps(scene))

    def run(*args, success=True):
        result = subprocess.run([binary, "-i", str(source), "-q", *args],
                                cwd=root, capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        return result

    run("-o", "nested/results", "-s", "snapshots/run.pmsnap")
    assert (root / "snapshots/run.pmsnap").is_file()
    field = root / "nested/results/simulation_results_1.csv"
    rows = list(csv.reader(field.open()))
    assert rows[0] == ["x[m]", "y[m]", "z[m]", "Hx[A/m]", "Hy[A/m]", "Hz[A/m]"]
    assert len(rows) == 9 and all(len(row) == 6 for row in rows)
    assert rows[1][:3] == ["0.100000001"] * 3
    forces = list(csv.reader((root / "nested/results/force_results.csv").open()))
    assert forces[1][0] == "16777217", "Magnet IDs must not pass through float"
    original = field.read_bytes()
    run("-o", "nested/results", success=False)
    assert field.read_bytes() == original
    run("-o", "nested/results", "--overwrite")
    run("-o", "legacy", "--legacy-csv")
    assert len(next(csv.reader((root / "legacy/simulation_results_1.csv").open()))) == 3
    run("-o", str(source), success=False)
    run("-s", str(source), "--overwrite", success=False)
    run("-o", "same", "-s", "same/force_results.csv", success=False)
    assert json.loads(source.read_text()) == scene

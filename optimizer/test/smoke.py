"""Check optimizer exports, verification metadata, and checkpoint continuation."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory(prefix="paramagnetics-optimizer-") as directory:
    root = Path(directory)
    spec = {
        "ring_count": 3, "ring_separation": 0.03,
        "candidates": [{"radius": 0.10, "count": 8}, {"radius": 0.12, "count": 10}],
        "outer_radius_offset": 0.02, "outer_count_offset": 2,
        "element": {"size": 0.01, "remanence": 1.3},
        "dsv": 0.04, "resolution": 0.01,
        "genetic": {"population": 16, "generations": 4, "seed": 7},
    }
    config = root / "config.json"
    config.write_text(json.dumps(spec))

    def run(output, *args, success=True):
        result = subprocess.run([binary, "-c", str(config), "-o", output, "-q", *args],
                                cwd=root, capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stdout + result.stderr
        if success:
            return json.loads((root / output).read_text())["halbach_optimization"]

    run("partial.json", "-g", "2", "--checkpoint", "state.json", "--no-verify")
    resumed = run("resumed.json", "--resume", "state.json", "--checkpoint", "state.json")
    full = run("full.json", "--field", "field.csv", "--history", "history.csv")
    assert resumed["genome"] == full["genome"]
    assert resumed["verified"] == full["verified"]
    assert full["verification"]["resolution_m"] == 0.005
    assert full["verified"]["points"] > full["verification"]["coarse"]["points"]
    assert full["verification"]["shifted"]["points"] > 0
    assert (root / "field.csv").is_file() and (root / "history.csv").is_file()
    run("invalid.json", "--checkpoint-every", "0", success=False)
    run("state.json", "--checkpoint", "state.json", success=False)
    spec["element"]["remanence"] = 1.2
    config.write_text(json.dumps(spec))
    run("wrong.json", "--resume", "state.json", success=False)

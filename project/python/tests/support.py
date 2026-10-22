"""Shared helpers for the end-to-end tests."""
import json
import os
import subprocess
import sys
from pathlib import Path

PYTHON_DIR = Path(__file__).resolve().parent.parent
PROJECT_ROOT = Path(os.environ.get("PROJECT_ROOT", PYTHON_DIR.parent))
SATELLITE = PROJECT_ROOT / "configs" / "satellite_support.json"
HYPOTHESES = PROJECT_ROOT / "experiments" / "hypotheses"
ANALYZE = os.environ.get("ANALYZE_BIN", str(PROJECT_ROOT / "build" / "analyze"))

sys.path.insert(0, str(PYTHON_DIR))


def analyze_available():
    return Path(ANALYZE).is_file() and os.access(ANALYZE, os.X_OK)


def run(*args, env=None):
    """Run the analyze tool; return (exit code, stdout, stderr). `env` adds environment variables."""
    full_env = {**os.environ, **env} if env else None
    done = subprocess.run([ANALYZE, *map(str, args)], capture_output=True, text=True, env=full_env)
    return done.returncode, done.stdout, done.stderr


def run_json(*args):
    code, out, err = run(*args)
    assert code == 0, f"analyze failed with exit {code}: {err}"
    return json.loads(out)


def without_timing(output):
    return {k: v for k, v in output.items() if k != "timing"}


def write_json(path, document):
    Path(path).write_text(json.dumps(document))
    return str(path)

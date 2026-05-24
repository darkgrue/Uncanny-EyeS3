import subprocess
import sys
from pathlib import Path

Import("env")

try:
    import numpy
except ImportError:
    print("[gen_eyes_all] Pillow library required. Installing prerequisites...")
    env.Execute("$PYTHONEXE -m pip install --upgrade pip")
    env.Execute("$PYTHONEXE -m pip install numpy")
    sys.exit(1)

# Use SCons environment to get project directory without __file__
PROJECT_DIR = Path(env["PROJECT_DIR"])


def run_tablegen_all(target, source, env):
    """Custom action: regenerate all display tables."""
    script = PROJECT_DIR / "resources/tools/tablegen.py"

    if not script.exists():
        print(f"[gen_display_tables_all] Error: {script} not found")
        return 1

    print("[gen_display_tables_all] Running: python tablegen.py -all")
    result = subprocess.run(
        [sys.executable, str(script), "-all"],
        cwd=PROJECT_DIR
    )
    return result.returncode


# Register custom target: pio run --target gen_display_tables
env.AddCustomTarget(
    "gen_display_tables_all",
    None,
    [run_tablegen_all],
    title="Regenerate Display Tables",
    description="Runs tablegen.py -all to regenerate all display table headers"
)

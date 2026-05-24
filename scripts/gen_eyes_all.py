import subprocess
import sys
from pathlib import Path

Import("env")

print("[gen_eyes_all] Installing prerequisites...")
env.Execute("$PYTHONEXE -m pip install Pillow")

# Use SCons environment to get project directory without __file__
PROJECT_DIR = Path(env["PROJECT_DIR"])


def run_geneye_all(target, source, env):
    """Custom action: regenerate all eye headers."""
    script = PROJECT_DIR / "resources/tools/geneye.py"
    if not script.exists():
        print(f"[gen_eyes_all] Error: {script} not found")
        return 1

    print("[gen_eyes_all] Running: python geneye.py -all")
    result = subprocess.run(
        [sys.executable, str(script), "-all"],
        cwd=PROJECT_DIR
    )
    return result.returncode


# Register custom target: pio run --target gen_eyes_all
env.AddCustomTarget(
    "gen_eyes_all",
    None,
    [run_geneye_all],
    title="Regenerate All Eye Headers",
    description="Runs geneye.py -all to regenerate all eye header files"
)

# set_build_version.py — PlatformIO pre-build script
# Injects BUILD_VERSION="YYYYMMDD-HHmmss-abcdef" as a compile flag.
import subprocess
import datetime

Import("env")

def get_git_hash():
    try:
        result = subprocess.run(
            ["git", "rev-parse", "--short=6", "HEAD"],
            capture_output=True, text=True, timeout=5
        )
        h = result.stdout.strip()
        return h if h else "000000"
    except Exception:
        return "000000"

# Inject immediately at script-load time so the define is present
# before any .cpp file is compiled (AddPreAction("buildprog") fires
# too late — after compilation, during linking).
now = datetime.datetime.now()
stamp = now.strftime("%Y%m%d-%H%M%S")
git_hash = get_git_hash()
version = "{}-{}".format(stamp, git_hash)
print("[build version] {}".format(version))
env.Append(CPPDEFINES=[("BUILD_VERSION", '\\"{}\\"'.format(version))])

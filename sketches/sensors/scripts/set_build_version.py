# set_build_version.py — PlatformIO pre-build script
# Injects BUILD_VERSION="YYYYMMDD-HHmmss-abcdef1" as a compile flag.
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

def set_build_version(source, target, env):
    now = datetime.datetime.now()
    stamp = now.strftime("%Y%m%d-%H%M%S")
    git_hash = get_git_hash()
    version = "{}-{}".format(stamp, git_hash)
    print("[build version] {}".format(version))
    env.Append(CPPDEFINES=[("BUILD_VERSION", '\\"{}\\"'.format(version))])

env.AddPreAction("buildprog", set_build_version)

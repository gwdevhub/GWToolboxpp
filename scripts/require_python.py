"""Imported first by every build script, so it must run on any Python, 2 included. Run directly, it prints the
interpreter's path when it is new enough, which is how the bootstrap scripts pick one."""

import sys

MINIMUM = (3, 9)

if sys.version_info < MINIMUM:
    sys.exit("error: these scripts need Python %d.%d or newer, but %s is Python %d.%d. Run them with a newer one, e.g. "
             "py -3 on Windows or python3 on Linux." % (MINIMUM + (sys.executable or "this interpreter",)
                                                       + tuple(sys.version_info[:2])))

if __name__ == "__main__":
    print(sys.executable)

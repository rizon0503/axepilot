# Feeds espota the OTA password from include/secrets.h (#10), so it never
# has to appear in platformio.ini, which is tracked by git unlike secrets.h.
Import("env")
import re

with open("include/secrets.h") as f:
    match = re.search(r'#define\s+OTA_PASSWORD\s+"([^"]*)"', f.read())

if match and match.group(1):
    env.Append(UPLOAD_FLAGS=["--auth=" + match.group(1)])
else:
    print("ota_auth.py: no OTA_PASSWORD in include/secrets.h — espota will fail auth")

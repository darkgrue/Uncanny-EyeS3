Import("env")

my_flags = env.ParseFlags(env['BUILD_FLAGS'])

#import pprint
#print("[rename_firmware] Parsed BUILD_FLAGS:")
#pprint.pprint(my_flags)

defines = dict()
for b in my_flags.get("CPPDEFINES"):
    if isinstance(b, list):
        defines[b[0]] = b[1]
    else:
        defines[b] = b

use_separator = False
ver_suffix = ""
if defines.get("DEBUG") or defines.get("FDEBUG") or defines.get("TERMINAL"):
    ver_suffix += "_"
    if defines.get("DEBUG") or defines.get("FDEBUG"):
        if (use_separator):
            ver_suffix += "-"
        ver_suffix += "debug"
        use_separator = True
    if defines.get("TERMINAL"):
        if (use_separator):
            ver_suffix += "-"
        ver_suffix += "terminal"
        use_separator = True
    if defines.get("ENABLE_RGB"):
        if (use_separator):
            ver_suffix += "-"
        ver_suffix += "rgb"
        use_separator = True

env.Replace(PROGNAME="%s_v%s%s" % (
    env["PIOENV"],
    defines.get("BUILDVER"),
    ver_suffix
))
print("[rename_firmware] Firmware binary name prefix is:", env['PROGNAME'])

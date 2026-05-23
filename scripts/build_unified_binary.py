# Combines separate bin files with their respective offsets into a single file
# This single file must then be flashed to an ESP32 node with 0x0 offset.
from os.path import join
import subprocess
import sys
Import("env")

if "IsCleanTarget" in dir(env) and env.IsCleanTarget():
    # Don't run the action during 'pio clean'
    Exit(0)

platform = env.PioPlatform()


# print("[unified] Installing prerequisites...")
# env.Execute("$PYTHONEXE -m pip install intelhex")


def esp32_create_combined_bin(source, target, env):
    print("[unified] Generating combined binary for serial flashing...")

    # The offset from begin of the file where the app0 partition starts
    # This is defined in the partition .csv file
    app_offset = env.get("ESP32_APP_OFFSET")

    object_file_name = env.subst(
        "$PROJECT_DIR/esp-web-tools/firmware/${PROGNAME}_combined.bin")
    sections = env.subst(env.get("FLASH_EXTRA_IMAGES"))
    firmware_name = env.subst("$BUILD_DIR/${PROGNAME}.bin")
    chip = env.get("BOARD_MCU")
    flash_size = env.BoardConfig().get("upload.flash_size", "4MB")
    flash_mode = env["__get_board_flash_mode"](env)
    flash_freq = env["__get_board_f_flash"](env)

    cmd = [
        "--chip",
        chip,
        "merge-bin",
        "-o",
        object_file_name,
        "--flash-mode",
        flash_mode,
        "--flash-freq",
        flash_freq,
        "--flash-size",
        flash_size,
    ]

    print("    Offset | File")
    for section in sections:
        sect_adr, sect_file = section.split(" ", 1)
        print(f" -  {sect_adr} | {sect_file}")
        cmd += [sect_adr, sect_file]

    print(f" - {app_offset} | {firmware_name}")
    cmd += [app_offset, firmware_name]

    print('[unified] Using esptool.py arguments: %s' % ' '.join(cmd))

    subprocess.run([sys.executable, "-m", "esptool"] + cmd, check=True)


# TRIGGER: Target the specific firmware file instead of "buildprog"
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", esp32_create_combined_bin)

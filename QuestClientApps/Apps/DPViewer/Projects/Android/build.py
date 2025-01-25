#!/usr/bin/env python3

import subprocess
import time
import argparse

PACKAGE_NAME = "app.wiselab.DPViewer"
ACTIVITY_NAME = "android.app.NativeActivity"

def build():
    print("Building APK...")
    subprocess.run(["./gradlew", "assembleDebug"])

def build_and_install():
    print("Building and installing APK...")
    subprocess.run(["./gradlew", "installDebug"])

def launch_activity():
    print("Launching app...")
    subprocess.run(["adb", "shell", "am", "start", "-n", f"{PACKAGE_NAME}/{ACTIVITY_NAME}"])

def start_logcat():
    print("Starting adb logcat...")
    subprocess.run(['adb', 'shell', 'logcat', f'--pid=$(pidof -s {PACKAGE_NAME})'])

def main(args):
    if args.build or (not args.build and not args.launch and not args.debug):
        build()
    if args.launch:
        build_and_install()
        time.sleep(1)
        launch_activity()
    if args.debug:
        build_and_install()
        time.sleep(1)
        launch_activity()
        time.sleep(1)
        start_logcat()

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build, install, and debug an Android app")
    parser.add_argument("--build", action="store_true", help="Build the APK")
    parser.add_argument("--launch", action="store_true", help="Build and launch the APK")
    parser.add_argument("--debug", action="store_true", help="Build, launch, and debug the app")
    args = parser.parse_args()

    try:
        main(args)
    except KeyboardInterrupt:
        print("Exiting...")

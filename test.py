# coding: utf-8

import os
import re
import sys
import platform
import subprocess


def run_command(cmd):
    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
    )
    output, error = process.communicate()
    return output.decode(), error.decode(), process.returncode


def check_gcc_version():
    output, err, code = run_command("gcc --version")
    if "5.5.0" in output:
        print("GCC 5.5.0 installed")
        print(output)
        return True
    else:
        print("GCC 5.5.0 not installed")
        print(output)
        return False


# install gcc 5.5.0 on Ubuntu 18.04
distribution, version, _ = platform.linux_distribution()
if distribution == "Ubuntu" and version == "18.04" and not check_gcc_version():
    os.system("sudo apt update && sudo apt install -y gcc-5 g++-5")
    os.system("sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10")
    os.system("sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 20")
    os.system("sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10")
    os.system("sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 20")
    os.system("sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30")
    os.system("sudo update-alternatives --set cc /usr/bin/gcc")
    os.system("sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30")
    os.system("sudo update-alternatives --set c++ /usr/bin/g++")
    if not check_gcc_version():
        print("Install GCC 5.5.0 failed")
        sys.exit(1)
else:
    print("Not running on Ubuntu 18.04. GCC 5.5.0 will not be installed.")

# cmake
test_dir = "cmake-build-debug"
if not os.path.exists(test_dir):
    os.mkdir(test_dir)
os.chdir(test_dir)
output, err, code = run_command("cmake ..")
print(output)
print(err)
if code != 0:
    print("return code:", code)
    sys.exit(1)
output, err, code = run_command("cmake --build ./")
print(output)
print(err)
if code != 0:
    print("return code:", code)
    sys.exit(1)

# run test


def print_test(test, output, err):
    print("========= {} ==========".format(test))
    print(output)
    print(err)


tests = []
fail_tests = []
for f in os.listdir():
    if re.match(".*?test[^\.]*?", f):
        tests.append(os.path.abspath(f))
for i, test in enumerate(tests):
    print("Running {}/{} tests...".format(i + 1, len(tests)), end="\r")
    output, err, code = run_command(test)
    if code != 0 or err or "test case failed" in output.lower():
        fail_tests.append((test, output, err))
    else:
        print_test(test, output, err)

print("\nFAILED TESTS:")
for test, output, err in fail_tests:
    print_test(test, output, err)

print("=======================================")
print(
    "TOTAL: {}, SUCCESS: {}, FAIL: {}".format(
        len(tests), len(tests) - len(fail_tests), len(fail_tests)
    )
)

if len(fail_tests) == 0:
    sys.exit(0)
else:
    sys.exit(1)

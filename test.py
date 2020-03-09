# coding: utf-8

import os
import re
import sys
import argparse
import subprocess


arg_parser = argparse.ArgumentParser()
arg_parser.add_argument(
    "-p", "--prefix", action="store", help="run tests with specific prefix"
)
arg_parser.add_argument(
    "-d", "--directory", action="store", help="executable directories, eg. -d rbf,rm"
)
args = arg_parser.parse_args()
if args.directory:
    executable_dir = args.directory.split(",")
else:
    executable_dir = ["cmake-build-debug"]
for dir in executable_dir:
    if not os.path.exists(dir):
        print(executable_dir, "not found")
        sys.exit(1)

test_orders = [
    "rbftest\_\d+",
    "rbftest\_custom\_\d+",
    "rmtest_create_tables",
    "rmtest_delete_tables",
    "rmtest_create_tables",
    "rmtest\_\d+",
    "rmtest\_custom\_\d+",
    "ixtest\_\d+",
    # "ixtest\_extra\_\d+",
    "ixtest\_p\d+",
    # "ixtest\_pe\_\d+",
    "ixtest\_custom\_\d+",
    "qetest\_\d+",
    "qetest\_p\d+",
]

cmd = "{test_path}"


def run_command(cmd):
    result = subprocess.run(cmd, shell=True)
    return result.returncode


unordered_tests, fail_tests, success_tests = [], [], []
for dir in executable_dir:
    for f in os.listdir(dir):
        f = os.path.join(dir, f)
        name, type = os.path.splitext(os.path.basename(f))
        if (
            os.path.isfile(f)
            and re.match(".*?test[^\.]*?", f)
            and os.access(f, os.X_OK)
            and not type
        ):
            if args.prefix and not name.startswith(args.prefix):
                continue
            unordered_tests.append(os.path.abspath(f))
unordered_tests.sort()
tests = []
for order in test_orders:
    for name in unordered_tests:
        if re.search(order, name):
            tests.append(name)
print("ALL TESTS:")
for test in tests:
    print(test)

# run tests
print("Start running tests...")
for i, test in enumerate(tests):
    print("Running {}/{} tests...".format(i + 1, len(tests)), end="\r")
    code = run_command(cmd.format(test_path=test))
    if code != 0:
        fail_tests.append(test)
    else:
        success_tests.append(test)
    print("RETURN CODE", code, "\n")

print("=======================================")
print("Success Tests:")
for test in success_tests:
    print(os.path.basename(test))
print("=======================================")
print("Fail Tests:")
for test in fail_tests:
    print(os.path.basename(test))
print("=======================================")
print(
    "TOTAL: {}, SUCCESS: {}, FAIL: {}".format(
        len(tests), len(success_tests), len(fail_tests)
    )
)

if len(fail_tests) == 0:
    sys.exit(0)
else:
    sys.exit(1)

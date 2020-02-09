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
    executable_dir = args.directory.split(',')
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
]

cmd = "{test_path}"


def run_command(cmd):
    process = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
    )
    output, error = process.communicate()
    return output.decode(), error.decode(), process.returncode


def print_test(test, output, err, code):
    print("========= {} ==========".format(os.path.basename(test)))
    print(output, end="")
    if err:
        print(err, end="")
    print("Exit code:", code, end="\n\n")


unordered_tests, fail_tests, success_tests = [], [], []
for dir in executable_dir:
    for f in os.listdir(dir):
        f = os.path.join(dir, f)
        if os.path.isfile(f) and re.match(".*?test[^\.]*?", f) and os.access(f, os.X_OK):
            if args.prefix and not os.path.basename(f).startswith(args.prefix):
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
    output, err, code = run_command(cmd.format(test_path=test))
    if code != 0 or "test case failed" in output.lower():
        fail_tests.append((test, output, err))
    else:
        success_tests.append((test, output, err))
    print_test(test, output, err, code)

print("=======================================")
print("Success Tests:")
for test, _, _ in success_tests:
    print(os.path.basename(test))
print("=======================================")
print("Fail Tests:")
for test, _, _ in fail_tests:
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

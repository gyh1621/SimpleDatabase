# coding: utf-8

import os
import re
import sys
import subprocess


executable_dir = "cmake-build-debug"
if not os.path.exists(executable_dir):
    print(executable_dir, "not found")
    sys.exit(1)
os.chdir(executable_dir)

valgrind_cmd = (
    "G_SLICE=always-malloc G_DEBUG=gc-friendly  "
    + "valgrind -v --tool=memcheck --leak-check=full --num-callers=40 {test_path}"
)

tests_order_map = {"rbftest": "0", "rmtest_create_tables": "1", "rmtest": "2"}


def get_test_order(test_name):
    test_name = os.path.basename(test_name)
    name, suffix = os.path.splitext(test_name)
    if name in tests_order_map:
        return tests_order_map[name] + name
    prefix, left = name.split("_", 1)
    if prefix in tests_order_map:
        return tests_order_map[prefix] + name
    else:
        print("illegal test name format:" + test_name)
        sys.exit(1)


if len(sys.argv) > 1 and sys.argv[1] == "mem":
    cmd = valgrind_cmd
else:
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


tests, fail_tests, success_tests = [], [], []
for f in os.listdir():
    if (
        os.path.isfile(f)
        and re.match(".*?test[^\.]*?", f)
        and os.access(f, os.X_OK)
    ):
        tests.append(os.path.abspath(f))
tests.sort(key=get_test_order)
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

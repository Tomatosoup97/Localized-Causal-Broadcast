#!/usr/bin/env python3

import os
import subprocess
import sys
import time

PROCESS_NUM = 3
MESSAGES_COUNT = 100_000
VERBOSE = 0

PATHS = [
    f"/home/mu/EPFL/DA/project-repo/example/output/{i}.output"
    for i in range(1, PROCESS_NUM + 1)
]

EXPECTED_LINES = MESSAGES_COUNT * 4


def do_call(args):
    oneline = ['"{}"'.format(x) for x in args]
    oneline = " ".join(oneline)
    if VERBOSE:
        print("[{}]> {}".format(os.getcwd(), oneline))
    try:
        return subprocess.check_output(args, env=os.environ)
    except subprocess.CalledProcessError as error:
        print(error)
        print(error.output)
        sys.exit(1)


def get_num_of_lines(path):
    out = do_call(["sh", "-c", "wc -l " + path + " | awk '{print $1}'"])
    out = out.decode("utf-8").replace("\n", "")
    return int(out)


def check_outputs():
    for path in PATHS:
        lines = get_num_of_lines(path)
        if lines != EXPECTED_LINES:
            return False

    return True


if __name__ == "__main__":
    while not check_outputs():
        time.sleep(1)

    print("Finished successfully!")
    exit(0)

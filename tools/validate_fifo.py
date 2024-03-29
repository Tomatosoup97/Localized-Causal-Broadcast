#!/usr/bin/env python3

import argparse
import os

from collections import defaultdict


def check_positive(value):
    ivalue = int(value)
    if ivalue <= 0:
        raise argparse.ArgumentTypeError(
            "{} is an invalid positive int value".format(value)
        )
    return ivalue


def checkNextMessages(nextMessage, m, filePath):
    if m == 0:
        return True
    for proc, next_msg in nextMessage.items():
        msgs = next_msg - 1
        if msgs != m:
            print(
                f"File {filePath} does not match expected messages count. "
                f"Expected {m}, actual: {msgs}"
            )
            return False
    return True


def checkProcess(filePath, m):
    i = 1
    nextMessage = defaultdict(lambda: 1)
    filename = os.path.basename(filePath)

    with open(filePath) as f:
        for lineNumber, line in enumerate(f):
            tokens = line.split()

            # Check broadcast
            if tokens[0] == "b":
                msg = int(tokens[1])
                if msg != i:
                    print(
                        "File {}, Line {}: Messages broadcast out of order. "
                        "Expected message {} but broadcast message {}".format(
                            filename, lineNumber, i, msg
                        )
                    )
                    return False
                i += 1

            # Check delivery
            if tokens[0] == "d":
                sender = int(tokens[1])
                msg = int(tokens[2])
                if msg != nextMessage[sender]:
                    print(
                        "File {}, Line {}: Message delivered out of order. "
                        "Expected message {}, but delivered message {}".format(
                            filename, lineNumber, nextMessage[sender], msg
                        )
                    )
                    return False
                else:
                    nextMessage[sender] = msg + 1

    return checkNextMessages(nextMessage, m, filePath)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--proc_num",
        required=True,
        type=check_positive,
        dest="proc_num",
        help="Total number of processes",
    )

    parser.add_argument(
        "-m",
        required=True,
        type=int,
        dest="m",
        help="Total number of messages",
    )

    parser.add_argument("output", nargs="+")

    results = parser.parse_args()

    if len(results.output) != results.proc_num:
        print("Not as many output files as number of processes")
        exit(1)

    for o in results.output:
        print("Checking {}".format(o))
        if checkProcess(o, results.m):
            print("Validation OK")
        else:
            print("Validation failed!")

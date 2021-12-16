#!/usr/bin/env python3

# Copyright Fetch Robotics 2019
# Author: Steffen Fuchs

# std
import argparse
import datetime
import subprocess
import time

DURATION_RUN = 23*60*60
DURATION_SLEEP = 1*60*60


def run_cycle(run_duration, sleep_duration, prefix, binary):
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    filename = f"{prefix}{timestamp}.log"

    with open(filename, 'w') as logfile:
        print(f"Writing to {filename}")
        proc = subprocess.Popen(binary.split(), stdout=logfile, stderr=subprocess.STDOUT)
        print(f"Started process {binary} for {run_duration} seconds")
        time.sleep(run_duration)
        print(f"Stopping process {binary}")
        proc.kill()

    print(f"Sleep for {sleep_duration} seconds")
    time.sleep(sleep_duration)


def main():
    parser = argparse.ArgumentParser(description="Periodically restart camera tester")
    parser.add_argument("--run", type=int, default=DURATION_RUN, help="test duration in seconds")
    parser.add_argument("--sleep", type=int, default=DURATION_SLEEP, help="pause duration in seconds")
    parser.add_argument("--prefix", type=str, default="", help="log file prefix")
    parser.add_argument("--binary", type=str, default="bin/camera_tester", help="path to camera_tester binary")
    args = parser.parse_args()

    try:
        while True:
            run_cycle(args.run, args.sleep, args.prefix, args.binary)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()

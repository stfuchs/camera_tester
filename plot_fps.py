#!/usr/bin/env python3

import altair as alt
import argparse
import datetime
import pandas as pd
import numpy as np
import re

alt.renderers.enable("altair_viewer")
alt.data_transformers.disable_max_rows()


# Regex captures
DT_CAPTURE = r'(?P<datetime>\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{6})'
CAM_CAPTURE = r'(?P<serial>\d{12})'
MSG_CAPTURE = r'(?P<msg>.*)'
FPS_CAPTURE = r'FPS: (?P<fps>[\d\.]+) \((?P<images>\d+) / (?P<interval>[\d\.]+)\)'


def read_lines(filenames):
    for filename in filenames:
        with open(filename) as f:
            for line in f:
                yield line


def parse_lines(lines):
    regex = re.compile(r'^' + DT_CAPTURE + r' \[' + CAM_CAPTURE + r'\] ' + MSG_CAPTURE)

    def _parse_datetime(s):
        FORMAT = '%Y-%m-%d %H:%M:%S.%f'
        return datetime.datetime.strptime(s, FORMAT)

    for l in lines:
        res = regex.match(l)
        if res is not None:
            line_dict = res.groupdict()
            line_dict["datetime"] = _parse_datetime(line_dict["datetime"])
            yield line_dict
        else:
            print(f"Failed to parse the following line:\n{l}")


def parse_fps(lines):
    fps = re.compile(FPS_CAPTURE)

    for l in lines:
        res = fps.match(l["msg"])
        if res is not None:
            l.update(res.groupdict())
            l["fps"] = float(l["fps"])
            yield l


def plot_fps(filenames, resolution='2Min'):
    df = pd.DataFrame(parse_fps(parse_lines(read_lines(filenames))))
    df = df.groupby("serial").resample(resolution, on='datetime')["fps"].mean().reset_index()
    df.fillna(0)
    print(df)

    #alt.Chart(df).mark_bar().encode(
    alt.Chart(df).mark_area(line=True, opacity=0.3).encode(
        alt.X("datetime"),
        alt.Y("fps:Q"),
        alt.Row("serial:N"),
    ).properties(width=1200, height=60).show()


def main():
    parser = argparse.ArgumentParser(description="Parse and plot fps from camera_tester logs")
    parser.add_argument("logs", nargs='+', help="Logs file generated by camera_tester.cpp")
    parser.add_argument("--resolution", "-r", default="2Min", type=str, help="FPS plot resolution (e.g. 30S, 5Min, 1H)")
    args = parser.parse_args()

    plot_fps(args.logs, args.resolution)


if __name__ == '__main__':
    main()

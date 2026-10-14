#!/usr/bin/env python3
"""Extract K.11.4.1.1 profile rows from a console/engine log without averaging percentiles."""
import argparse
import csv
import math
from pathlib import Path
import re

HEADER = ['side', 'window', 'metric', 'unit', 'samples', 'mean', 'p50', 'p95', 'p99', 'max', 'calls']


def extract(text):
    rows = []
    for line in text.splitlines():
        start = line.find('PERF_K11411,')
        if start < 0:
            continue
        try:
            row = next(csv.reader([line[start:]]))
            if len(row) != 12 or row[1] not in ('client', 'server') or row[4] not in ('ms', 'count', 'hz'):
                continue
            if not re.fullmatch(r'[a-z_][a-z0-9_]*', row[3]):
                continue
            if int(row[2]) < 1 or not 1 <= int(row[5]) <= 2048 or int(row[11]) < 0:
                continue
            if not all(math.isfinite(float(value)) and float(value) >= 0 for value in row[6:11]):
                continue
        except (ValueError, csv.Error):
            continue
        rows.append(row[1:])
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('csv', type=Path)
    args = parser.parse_args()
    rows = extract(args.log.read_text(encoding='utf-8', errors='replace'))
    if not rows:
        parser.error('No valid PERF_K11411 rows found; capture with r_profile 2 / sv_profile 2.')
    args.csv.parent.mkdir(parents=True, exist_ok=True)
    with args.csv.open('w', newline='', encoding='utf-8') as output:
        writer = csv.writer(output)
        writer.writerow(HEADER)
        writer.writerows(rows)
    print(f'Extracted {len(rows)} rows to {args.csv}')


if __name__ == '__main__':
    main()

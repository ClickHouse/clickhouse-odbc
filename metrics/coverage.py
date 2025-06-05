"""
The script converts output of llvm-cov report to json representation.
The result then is printed to STDOUT. The output can be used to insert
it to the database.

In this case use the following table:

    create database odbc;
    create table odbc.code_coverage (
        commit String,
        timestamp DateTime32,
        filename String,
        regions UInt32,
        missed_regions UInt32,
        functions UInt32,
        missed_functions UInt32,
        lines UInt32,
        missed_lines UInt32,
        branches UInt32,
        missed_branches UInt32,
        inserted_at DateTime32 DEFAULT now()
    )
    engine MergeTree
    order by (timestamp, commit);

Usage example:

    llvm-cov report <binary> -instr-profile=<your.profdata> > ./coverage-report.txt
    python coverage.py coverage-report.txt \
      $(git describe --no-match --always --abbrev=0 --dirty --broken) \
      $(git show --no-patch --format=%ct HEAD) | \
      curl https://clickhouse.host.name:8443/?query=INSERT%20INTO%20odbc.code_coverage%20FORMAT%20JSONEachRow \
        -u "default:password" \
        --data-binary @-
"""

import re
import json
import sys

field_map = {
    "Filename": "filename",
    "Regions": "regions",
    "Missed Regions": "missed_regions",
    "Functions": "functions",
    "Missed Functions": "missed_functions",
    "Lines": "lines",
    "Missed Lines": "missed_lines",
    "Branches": "branches",
    "Missed Branches": "missed_branches",
}

def main(file_name: str, commit: str, timestamp: int):
    with open(file_name, 'r') as file:
        rows = file.readlines()

    header = rows[0]

    columns = {}
    start = 0
    for match in re.finditer(r"\s\s+|\n", header):
        fr, to = match.span()
        field = header[start:fr]

        value = (0, 0)
        if field == "Filename":
            value = (start, to)
        else:
            value = (start, fr)

        start = to

        if field in field_map:
            columns[field_map[field]] = value

    min_length = start

    data = []
    for row in rows[2:]:
        if len(row) < min_length:
            continue
        if row.startswith("-----"):
            continue

        filename = ""
        item = {"commit": commit, "timestamp": timestamp}
        for field, [fr, to] in columns.items():
            value = row[fr:to].strip()
            if field == "filename":
                item[field] = value
            else:
                item[field] = int(value)

        if not item["filename"] == "TOTAL":
            data.append(item)

    print(json.dumps(data, indent=2))

if __name__ == "__main__":
    if len(sys.argv) < 4:

        print(f"usage: {sys.argv[0]} <coverage-report.txt> <commit> <commit timestamp>")
        exit(1)
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]))

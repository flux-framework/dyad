from pathlib import Path
import re
import argparse
import json


def validate_log(out_file):
    with open(str(out_file), "r") as f:
        for line in f:
            if line.startswith("[DSPACES_TEST]"):
                return line
    return None


def validate_dir(path):
    dirname = path.name
    match_obj = re.match(r"(?P<test_name>[a-zA-Z]+)_(?P<num_nodes>[0-9]+)_(?P<ppn>[0-9]+)", dirname)
    if match_obj is None:
        raise RuntimeError("Cannot parse directory name")
    test_name = match_obj.group("test_name")
    num_nodes = int(match_obj.group("num_nodes"))
    ppn = int(match_obj.group("ppn"))
    # num_tasks = num_nodes * ppn
    out_file = path / "run.out"
    if not out_file.is_file():
        raise RuntimeError("Could not find run.out for {}".format(path))
    perf_line = validate_log(out_file)
    if perf_line is None:
        raise RuntimeError("Run for {} failed because we don't have perf numbers".format(path))
    return {
        "test_name": test_name,
        "num_nodes": num_nodes,
        "ppn": ppn,
        "perf": perf_line,
    }


def validate_rundir(td):
    print("Validating tests in {}:".format(td.name))
    subdirs = [sd for sd in td.iterdir() if sd.is_dir()]
    perf_entries = []
    for sd in subdirs:
        print("  * Validating {}:".format(sd.name), end=" ")
        try:
            new_perf = validate_dir(sd)
            perf_entries.append(new_perf)
            print("GOOD")
        except RuntimeError as e:
            print("BAD")
            raise e
    return perf_entries


def main():
    parser = argparse.ArgumentParser("Validate runs")
    parser.add_argument("testdir", type=Path,
                        help="Top-level directory representing the results of a single iteration of the testing")
    parser.add_argument("--dump_file", "-d", type=Path, default=None,
                        help="Path to JSON file where we want to dump performance results")
    args = parser.parse_args()
    perf_entries = validate_rundir(args.testdir.expanduser().resolve())
    if args.dump_file is not None:
        dump_file = args.dump_file.expanduser().resolve()
        if not dump_file.name.endswith(".json"):
            raise ValueError("Invalid file suffix for JSON file")
        if not dump_file.parent.is_dir():
            dump_file.parent.mkdir(parents=True)
        with open(str(dump_file), "w") as f:
            json.dump(perf_entries, f, indent=4, sort_keys=True)
    else:
        print(json.dumps(perf_entries, sort_keys=True, indent=4))


if __name__ == "__main__":
    main()

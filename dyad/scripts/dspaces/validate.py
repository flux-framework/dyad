from pathlib import Path
import re
import argparse


NUM_FILES = 16


def validate_csv(csv_file):
    with open(str(csv_file), "r") as f:
        num_lines = len(f.readlines())
        if num_lines != NUM_FILES + 1:
            raise RuntimeError("CSV file {} only contains {} lines, expected {}".format(csv_file, num_lines, NUM_FILES+1))


def validate_dir(path):
    dirname = path.name
    match_obj = re.match(r"(?P<test_name>[a-zA-Z]+)_(?P<num_nodes>[0-9]+)_(?P<ppn>[0-9]+)", dirname)
    if match_obj is None:
        raise RuntimeError("Cannot parse directory name")
    num_nodes = int(match_obj.group("num_nodes"))
    ppn = int(match_obj.group("ppn"))
    num_tasks = num_nodes * ppn
    csv_files = list(path.glob("*.csv"))
    if len(csv_files) != num_tasks:
        raise RuntimeError("Only found {} CSV files, but expected {} for {}".format(len(csv_files), num_tasks, str(path)))
    for f in csv_files:
        validate_csv(f)


def validate_rundir(td):
    print("Validating tests in {}:".format(td.name))
    subdirs = [sd for sd in td.iterdir() if sd.is_dir()]
    for sd in subdirs:
        print("  * Validating {}:".format(sd.name), end=" ")
        try:
            validate_dir(sd)
            print("GOOD")
        except RuntimeError as e:
            print("BAD")
            raise e


def main():
    parser = argparse.ArgumentParser("Validate runs")
    parser.add_argument("testdir", type=Path,
                        help="Top-level directory representing the results of a single iteration of the testing")
    args = parser.parse_args()
    validate_rundir(args.testdir.expanduser().resolve())


if __name__ == "__main__":
    main()

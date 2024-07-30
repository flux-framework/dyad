from pathlib import Path
import re
import argparse
import pandas as pd


def process_single_run_csvs(dir_path):
    dirname = dir_path.name
    match_obj = re.match(r"(?P<test_name>[a-zA-Z]+)_(?P<num_nodes>[0-9]+)_(?P<ppn>[0-9]+)", dirname)
    if match_obj is None:
        raise RuntimeError("Cannot parse directory name")
    num_nodes = int(match_obj.group("num_nodes"))
    ppn = int(match_obj.group("ppn"))
    csv_files = list(dir_path.glob("*.csv"))
    df = pd.concat(map(pd.read_csv, csv_files), ignore_index=True)
    num_ops = len(df)
    df = df.drop(columns=["var_name", "version"])
    df = df.groupby("rank").agg("sum")
    return {
        "test_name": match_obj.group("test_name"),
        "num_nodes": num_nodes,
        "ppn": ppn,
        "num_mdata_ops": num_ops,
        "data_size": df["data_size"].sum(),
        "mdata_time_ns": df["mdata_time_ns"].max(),
        "data_time_ns": df["data_time_ns"].max(),
    }


def build_result_dataframe(testdir):
    top_level_rundir_name = testdir.parent.name
    test_dir_name = testdir.name
    output_df_name = "{}_{}.csv".format(top_level_rundir_name, test_dir_name)
    print("Building", output_df_name)
    df_rows = []
    for subdir in testdir.iterdir():
        if subdir.is_dir():
            print("Getting data for", str(subdir))
            df_row = process_single_run_csvs(subdir)
            df_rows.append(df_row)
    output_df = pd.DataFrame(data=df_rows)
    return output_df_name, output_df


def main():
    parser = argparse.ArgumentParser("Aggregate data for test")
    parser.add_argument("testdir", type=Path,
                        help="Path to the test directory to collect data for")
    parser.add_argument("--dump_dir", "-d", type=Path,
                        help="Directory to dump the resulting CSV into")
    args = parser.parse_args()
    csv_name, df = build_result_dataframe(args.testdir.expanduser().resolve())
    dump_dir = args.dump_dir.expanduser().resolve()
    if not dump_dir.is_dir():
        print("Creating non-existant dump directory {}".format(str(dump_dir)))
        dump_dir.mkdir(parents=True)
    full_csv_name = dump_dir / csv_name
    df.to_csv(str(full_csv_name))
    print("Wrote data to {}".format(str(full_csv_name)))


if __name__ == "__main__":
    main()

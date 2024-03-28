import sysconfig
import site
from pathlib import Path
import argparse
import re


def get_sitepackages_for_dspaces():
    env_sitepackages = sysconfig.get_path("platlib")
    match_obj = re.match(r"^.*(?P<site>lib/.*)$", env_sitepackages)
    if match_obj is None:
        raise FileNotFoundError("Could not locate site-packages for venv")
    return match_obj.group("site")


def main():
    parser = argparse.ArgumentParser("Adds DataSpaces's Python bindings to venv")
    parser.add_argument("dspaces_install_prefix", type=Path,
                        help="Path to the DataSpaces install")
    parser.add_argument("--dspaces_sitepackages_dir", "-d", type=str,
                        default=get_sitepackages_for_dspaces(),
                        help="Override default path from DataSpaces install prefix to Python bindings")
    parser.add_argument("--venv_sitepackages_dir", "-v", type=Path,
                        default=Path(site.getsitepackages()[0]).expanduser().resolve(),
                        help="Override path to venv's site-packages directory")
    parser.add_argument("--pth_filename", "-p", type=str,
                        default="dspaces.pth",
                        help="Override the default name of the pth file that will be creating")
    args = parser.parse_args()
    dspaces_install_prefix = args.dspaces_install_prefix.expanduser().resolve()
    dspaces_sitepackages_dir = dspaces_install_prefix / args.dspaces_sitepackages_dir
    pth_file_contents = str(dspaces_sitepackages_dir) + "\n"
    full_pth_filename = args.venv_sitepackages_dir / args.pth_filename
    with open(str(full_pth_filename), "w") as f:
        f.write(pth_file_contents)
        

if __name__ == "__main__":
    main()

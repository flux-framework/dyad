import argparse
import os
import paramiko
import logging
import shutil
import json

def parse_args():
    parser = argparse.ArgumentParser(
                    prog='mpi_attach',
                    description='Attach to a mpi program')
    parser.add_argument('-c', '--conf_dir', help="pass conf_dir else it is infered using VSC_DEBUG_CONF_DIR")
    parser.add_argument('-p', '--project_dir', help="pass project_dir")
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('-d', '--debug', action='store_true')
    args = parser.parse_args()
    if  args.conf_dir == None:
        args.conf_dir = os.getenv("VSC_DEBUG_CONF_DIR", ".")
    loglevel = logging.WARNING
    if args.verbose:
        loglevel = logging.INFO
    elif args.debug:
        loglevel = logging.DEBUG
    logging.basicConfig(level=loglevel,
        handlers=[
            logging.StreamHandler()
        ],
        format='[%(levelname)s] [%(asctime)s] %(message)s [%(pathname)s:%(lineno)d]',
        datefmt='%H:%M:%S'
    )
    logging.info(f"args: {args}")
    return args

def main():
    args = parse_args()
    conf_file = f"{args.conf_dir}/debug.conf"
    file = open(conf_file, 'r')
    lines = file.readlines()
    file.close()

    vals = [{}]*len(lines)
    logging.info(f"vals has {len(vals)} values")
    for line in lines:
        exec, rank, hostname, port, pid = line.split(":")
        exec = exec.strip()
        rank = int(rank.strip())
        hostname = hostname.strip()
        port = int(port.strip())
        pid = int(pid.strip())
        vals[rank] = {"hostname":hostname, "port":port, "pid":pid, "exec":exec}

    # Create a launch_json files
    launch_file = f"{args.project_dir}/.vscode/launch.json"
    with open(launch_file, "r") as jsonFile:
        launch_data = json.load(jsonFile)
    
    # clean previous configurations
    confs = launch_data["configurations"]
    final_confs = []
    for conf in confs:
        if "mpi_gdb" not in conf["name"]:
            final_confs.append(conf)
    
    for rank, val in enumerate(vals):
        exec = val["exec"]
        port = val["port"]
        hostname = val["hostname"]
        final_confs.append({
            "type": "gdb",
            "request": "attach",
            "name": f"mpi_gdb for rank {rank}",
            "executable": f"{exec}",
            "target": f"{hostname}:{port}",
            "remote": True,
            "cwd": "${workspaceRoot}", 
            "gdbpath": "gdb"
        })
    launch_data["configurations"]=final_confs
    with open(launch_file, "w") as jsonFile:
        json.dump(launch_data, jsonFile, indent=2)

    gdbserver_exe = shutil.which("gdbserver")
    ssh = paramiko.SSHClient()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    for rank, val in enumerate(vals):
        hostname, port, pid = val["hostname"], val["port"], val["pid"]
        logging.info(f"rank:{rank} hostname:{hostname} port:{port} pid:{pid}")
        ssh.connect(hostname)
        cmd = f"{gdbserver_exe} {hostname}:{port} --attach {pid} > {os.getcwd()}/gdbserver-{hostname}-{pid}.log 2>&1 &"
        logging.info(f"cmd:{cmd}")
        transport = ssh.get_transport()
        channel = transport.open_session()
        channel.exec_command(cmd)
        ssh.close()

    
if __name__ == "__main__":
    main()
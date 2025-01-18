#!/bin/env python3

import argparse
import json
import subprocess
import re
import matplotlib.pyplot as plt
import pathlib

def get_results(config_filename):
    with open(args.config_filename, 'r') as f:
        cfg = json.loads(f.read())


    # Regex patterns for extracting data
    time_pattern = r'Time ([\d]+)/*[\d]*:'
    # Match any number of "name: number" pairs separated by |
    pop_pattern = r'{(.*)}'
    name_size_pattern = r'"([^"]+)":\s*(\d+)'

    with subprocess.Popen(
        ["./ecosystem", args.config_filename],
        text=True,
        stdout=subprocess.PIPE) as process:

        colors = {
            p['name'] : ('#' + p['color'].replace("'",""))
            for p in cfg['populations']}
        steps = []
        pops = {name : [] for name in colors.keys()}
        for line in process.stdout:
            if not line:
                break
            time_match = re.search(time_pattern, line)
            if not time_match:
                continue
            steps.append(int(time_match.group(1)))

            pop_match = re.search(pop_pattern, line)
            pop_data = pop_match.group(1)
            for m in re.finditer(name_size_pattern, pop_data):
                name = m.group(1)
                count = int(m.group(2))
                pops[name].append(count)

    return steps, pops, colors


def plot_populations(steps, pops, colors, img_filename=None):
    plt.style.use('dark_background')
    plt.figure(figsize=(10, 6))
    
    for name, population in pops.items():
        plt.plot(steps, population, color=colors[name], label=name)
    
    plt.xlabel('Time Step')
    plt.ylabel('Population')
    plt.title('Populations Over Time')
    plt.legend()
    plt.grid(True)
    plt.grid(True, alpha=0.5, linewidth=0.5)

    if img_filename:
        plt.savefig(img_filename, bbox_inches='tight', facecolor='black', dpi=120)
    else:
        plt.show()

if __name__ == "__main__":
    arg_parser = argparse.ArgumentParser(
        description='Plot population sizes for ecosystem simulator')
    arg_parser.add_argument('config_filename', type=str,
                    help='JSON configuration file describing simulation parameters')
    args = arg_parser.parse_args();

    steps, pops, colors = get_results(args.config_filename)
    img_filename = pathlib.Path(args.config_filename).stem + '_plot.png'
    plot_populations(steps, pops, colors, img_filename)

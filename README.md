Ecosystem Simulator
===================

This is a simple simulation of population dynamics.

Examples
--------

Here's a screenshot of a simulation of [grass, rabbits, and foxes](config/grass_rabbit_fox.json):

![screenshot](results/grass_rabbit_fox_screenshot.png)
![plot](results/grass_rabbit_fox_plot.png)

Here's a screenshot of a simulation of [an ecosystem with five organisms](config/balance.json):

![screenshot](results/balance_screenshot.png)
![plot](results/balance_plot.png)

Here's a screenshot of a simulation of [trees and beetles](config/tree_beetle.json):

![screenshot](results/tree_beetle_screenshot.png)
![plot](results/tree_beetle_plot.png)


Usage
-----

Building on linux:

    $ make
    
Building on Windows: (You'll need a recent-ish version of MSVC with support for C17.)

    $ .\build.bat

The simulation parameters are loaded from a JSON configuration file, which specifies how each population behaves. See
[config/](config/) for examples.

Running:

    $ ./build/ecosystem <config_file.json>

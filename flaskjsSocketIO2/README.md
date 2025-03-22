# Introduction

This directory contains example software to visualise location information on a floor map.
It is used to demonstrate the use of flask, javascript and visualisation. There are many alternatives and depending on detailed requirements a solution should be selected.


# Installation


First create a  virtuall environment for python using venv or anaconda.

```

> mkdir myproject
> cd myproject
> python -3 -m venv .venv`

```

Then  activate the environment

```

> .venv\Scripts\activate

```
After that flask can be installed:

```

> pip install flask

```
Install the socketio library for flask and the requests library:

```
pip install flask-socketio
pip install requests

```

Unzip the source file and you should see

```
./templates
./static/js
./static/images

```


# Usage


Open a terminal window in which the virtual environment(e.g. testUWB) is activated and Start webserver

(testUWB) python locfloor2.py
[{'type': 'S', 'x': 250, 'y': 300, 'z': 400}, {'type': 'S', 'x': 300, 'y': 350, 'z': 400}, {'type': 'T', 'x': 400, 'y': 400, 'z': 600}]
 * Serving Flask app 'locfloor2'
 * Debug mode: on
WARNING: This is a development server. Do not use it in a production deployment. Use a production WSGI server instead.
 * Running on all addresses (0.0.0.0)
 * Running on http://127.0.0.1:5000
 * Running on http://145.3.193.242:5000
Press CTRL+C to quit
 * Restarting with stat
[{'type': 'S', 'x': 250, 'y': 300, 'z': 400}, {'type': 'S', 'x': 300, 'y': 350, 'z': 400}, {'type': 'T', 'x': 400, 'y': 400, 'z': 600}]
 * Debugger is active!
 * Debugger PIN: 223-647-342

```

Use a webbrowser and select http://127.0.0.1:5000

There are a number of test scripts to experiment with a data interface to receive localisation data and interface to the browers visualisation. 

```
> python sendloopjson.py

[{'type': 'T', 'x': 250, 'y': 200, 'z': 400}, {'type': 'T', 'x': 400, 'y': 550, 'z': 400}, {'type': 'R', 'x': 450, 'y': 600, 'z': 600}, {'type': 'R', 'x': 550, 'y': 800, 'z': 600}]
JSON data sent successfully!
[{'type': 'T', 'x': 205, 'y': 155, 'z': 400}, {'type': 'T', 'x': 400, 'y': 550, 'z': 400}, {'type': 'R', 'x': 405, 'y': 555, 'z': 600}, {'type': 'R', 'x': 550, 'y': 800, 'z': 600}]
JSON data sent successfully!
[{'type': 'T', 'x': 165, 'y': 115, 'z': 400}, {'type': 'T', 'x': 400, 'y': 550, 'z': 400}, {'type': 'R', 'x': 365, 'y': 515, 'z': 600}, {'type': 'R', 'x': 550, 'y': 800, 'z': 600}]
JSON data sent successfully!
[{'type': 'T', 'x': 130, 'y': 80, 'z': 400}, {'type': 'T', 'x': 400, 'y': 550, 'z': 400}, {'type': 'R', 'x': 330, 'y': 480, 'z': 600}, {'type': 'R', 'x': 550, 'y': 800, 'z': 600}]

...
99

```










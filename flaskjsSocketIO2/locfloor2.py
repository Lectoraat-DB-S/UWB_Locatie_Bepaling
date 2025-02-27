# locfloor2.py
# demonstration of webtechnologies

from flask import Flask, render_template
from flask_socketio import SocketIO,emit
from flask import jsonify
from flask import request
import json
import random
import re
import sys
import threading
import time

# ---------------------------------------
# a Python object (dict) for test purposes
point = {
  "type": "S",
  "x": 250,
  "y": 300,
  "z": 400
}

point2 = {
  "type": "S",
  "x": 300,
  "y": 350,
  "z": 400
}

point3 = {
  "type": "T",
  "x": 400,
  "y": 400,
  "z": 600
}

points=list((point,point2,point3))
print(points)

#----------------------------------------------
# Origin reference in image
XIOrigin = 256
YIOrigin = 256
XOrigin = 0 
YOrigin = 0
ScaleXtoI = 0.1
ScaleYtoI = 0.1
# angle 
DiffAngle = 0
#
#------------------------------------------------
# Creation of ap
#
#-----------------------------------------------

app = Flask(__name__)

# 
# app.config['SECRET_KEY'] = 'secret!'
import secrets
app.config['SECRET_KEY'] = secrets.token_urlsafe(16)
socketio = SocketIO(app)

# global variable to communicate with javascript 
global datareceived

# load dummmy values
datareceived = points

#------------------------------------------------
# api call is used to
# receiving via api call data from interface
@app.route('/api/data', methods=['POST'])  
def api_data():
    if request.method == 'POST':
        # global datareceived
        datareceived = request.json 
        socketio.emit("UWBdata", {'data': datareceived})
        return 'JSON data received flask successfully!', 200
    else:
        return 'Method Not Allowed', 405

# Display your index page
@app.route("/")
def index():
    global datareceived
    return render_template("index.html",data=datareceived)

# receiving my_event
@socketio.on('update_event')
def handle_my_event(json):
    print('On update_event received JSON flask: ' + str(json))
    global datareceived
    socketio.emit('message',  datareceived)

@socketio.on('connect')
# def test_connect(auth):
def test_connect(h):
    print("Trying to connect from flask")
    emit('my response', {'data': 'Connected'})

@socketio.on('disconnect')
def test_disconnect():
    print('Client disconnected')

from flask_socketio import ConnectionRefusedError



@socketio.on('json')
def handle_json(json):
    print('received json flask: ' + str(json))




if __name__ == "__main__":
    # app.run(host='0.0.0.0', port=5000, debug=True)
    socketio.run(app,host='0.0.0.0', port=5000, debug=True)
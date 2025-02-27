from flask import Flask
from flask_socketio import SocketIO

app = Flask(__name__)
socketio = SocketIO(app)

@socketio.on('my_event')
def handle_my_event(json):
    print('Received JSON: ' + str(json))

if __name__ == '__main__':
    socketio.run(app, debug=True)

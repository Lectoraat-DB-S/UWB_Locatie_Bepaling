import socketio

# Create a Socket.IO client
sio = socketio.Client()

# Define the event handler for connection
@sio.event
def connect():
    print('Connection established')

# Define the event handler for disconnection
@sio.event
def disconnect():
    print('Disconnected from server')

# Connect to the server
sio.connect('http://localhost:5000')

# Send JSON data
data = {'message': 'Hello, server!'}
sio.emit('my_event', data)

# Disconnect from the server
sio.disconnect()
import eventlet
eventlet.monkey_patch()

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from flask_cors import CORS
import secrets
import threading
import socket
import logging
import time

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)

# ----------------------------------------------
# Config
FLASK_PORT = 5000
UDP_PORT = 8000

# ----------------------------------------------
# Flask app initialization
app = Flask(__name__)
CORS(app)  # allow all origins
app.config['SECRET_KEY'] = secrets.token_urlsafe(16)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode='eventlet')

received_data = {}
user_set_anchor_locations = {}


# ----------------------------------------------
@app.route('/api/data', methods=['POST'])
def api_data():
    global received_data

    received_data = request.json
    socketio.emit("UWBdata", {'data': received_data})
    return 'JSON data received flask successfully!', 200


@socketio.on('delete_anchor_positions')
def api_delete_anchor_locations(_):
    global user_set_anchor_locations

    logging.info("Clearing anchor positions")

    # Clear the user-set anchor locations
    user_set_anchor_locations = {}


@socketio.on('sync_anchor_positions')
def api_put_anchor_locations(js):
    global user_set_anchor_locations
    # Store the data directly without wrapping
    user_set_anchor_locations = js
    logging.info(f"User-set anchor locations: {user_set_anchor_locations}")


@socketio.on('get_anchor_positions')
def api_get_anchor_locations(_):
    global user_set_anchor_locations
    # For backward compatibility, continue wrapping in 'data'
    socketio.emit('anchor_positions', {'data': user_set_anchor_locations})


@app.route('/api/status', methods=['POST'])
def api_status():
    return jsonify({'success': True}), 200


# Display your index page
@app.route("/")
def index():
    return render_template("index.html", data=received_data)


@socketio.on('update_event')
def handle_my_event(_):
    socketio.emit('message', received_data)


@socketio.on('connect')
def test_connect(_):
    print("Trying to connect from flask")
    emit('my response', {'data': 'Connected'})


@socketio.on('disconnect')
def test_disconnect():
    print('Client disconnected')


def get_local_ip():
    """Get local IP address by connecting to an external server"""
    try:
        # Create a socket and connect to an external address (doesn't actually send data)
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        # Doesn't need to be reachable, just needs to be a valid address format
        s.connect(('8.8.8.8', 80))
        local_ip = s.getsockname()[0]
        s.close()
        return local_ip
    except Exception:
        return '127.0.0.1'  # Fallback to localhost if all else fails


def udp_server():
    global received_data
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(('0.0.0.0', UDP_PORT))
    logging.info(f"UDP server started on port {UDP_PORT}")

    # Data aggregation structures
    tag_buffers = {}  # Store partial data for each tag
    known_anchors = set()  # Track all known anchors
    last_update_times = {}  # Track when each tag was last updated
    last_emit_times = {}  # Track when we last emitted data for each tag

    # Configuration
    update_timeout = 1.0  # seconds to wait for complete data before emitting anyway

    try:
        while True:
            data, addr = sock.recvfrom(1024)
            data_str = data.decode('utf-8')
            current_time = time.time()

            # Handle discovery request
            if data_str == "DISCOVERY_REQUEST":
                local_ip = get_local_ip()
                response = f"Server IP: {local_ip}"
                logging.info(f"Sending server IP response to {addr}")
                sock.sendto(response.encode(), addr)
                continue

            # Handle error messages
            if data_str.startswith("ERROR:"):
                logging.warning(data_str)
                continue

            try:
                # Parse semicolon-separated string
                parts = data_str.split(';')
                if len(parts) >= 7:  # At least 1 tag + (3 anchors × 2 fields)
                    tag_id = parts[0]
                    anchors_in_message = []

                    # Initialize buffer for this tag if needed
                    if tag_id not in tag_buffers:
                        tag_buffers[tag_id] = {}

                    # Update last update time
                    last_update_times[tag_id] = current_time

                    # Process anchor data (pairs: MAC, distance)
                    for i in range(1, len(parts), 2):
                        if i + 1 < len(parts):
                            anchor_mac = parts[i]
                            
                            if anchor_mac == "000000":
                                # logging.warning(f"Invalid anchor MAC address: {anchor_mac}")
                                continue

                            known_anchors.add(anchor_mac)
                            anchors_in_message.append(anchor_mac)

                            try:
                                anchor_distance = float(parts[i + 1])
                                # Update buffer with this anchor's data
                                tag_buffers[tag_id][anchor_mac] = anchor_distance
                            except ValueError:
                                logging.error(f"Invalid distance value: {parts[i + 1]}")

                    # print(tag_buffers)

                    # Check if we should emit data
                    should_emit = False

                    # Condition 1: We have data for all known anchors
                    if len(tag_buffers[tag_id]) == len(known_anchors) and len(known_anchors) > 0:
                        logging.info(f"Tag {tag_id}: Complete data received for all {len(known_anchors)} anchors")
                        should_emit = True

                    # Condition 2: Timeout has occurred since last emission
                    elif tag_id in last_emit_times and current_time - last_emit_times[tag_id] > update_timeout:
                        logging.info(
                            f"Tag {tag_id}: Timeout occurred, emitting partial data for {len(tag_buffers[tag_id])}/{len(known_anchors)} anchors")
                        should_emit = True

                    # Condition 3: First update for this tag
                    elif tag_id not in last_emit_times:
                        logging.info(
                            f"Tag {tag_id}: First update, emitting data for {len(tag_buffers[tag_id])} anchors")
                        should_emit = True

                    # Emit aggregated data if conditions met
                    if should_emit and tag_buffers[tag_id]:
                        formatted_data = {
                            'tag_id': tag_id,
                            'anchors': []
                        }

                        # Build complete anchor list from buffer
                        for mac, distance in tag_buffers[tag_id].items():
                            formatted_data['anchors'].append({
                                'mac': mac,
                                'distance': distance
                            })

                        # Update shared data and emit to clients
                        received_data = formatted_data
                        socketio.emit('UWBdata', formatted_data)
                        socketio.emit('anchormacs', {'data': list(known_anchors)})
                        last_emit_times[tag_id] = current_time

                        logging.info(
                            f"Emitted data for tag {tag_id} with {len(formatted_data['anchors'])}/{len(known_anchors)} anchors")

            except Exception as e:
                logging.error(f"Error processing UDP data: {e}")

            # Clean up stale tag data (optional)
            stale_threshold = 60.0  # seconds
            stale_tags = [tag for tag, last_time in last_update_times.items()
                          if current_time - last_time > stale_threshold]
            for tag in stale_tags:
                if tag in tag_buffers:
                    del tag_buffers[tag]
                if tag in last_update_times:
                    del last_update_times[tag]
                if tag in last_emit_times:
                    del last_emit_times[tag]

    except Exception as e:
        logging.error(f"UDP server error: {e}")
    finally:
        sock.close()
        logging.info("UDP server stopped")


udp_thread = threading.Thread(
    target=udp_server,
    daemon=True
)
udp_thread.start()

if __name__ == "__main__":
    print('Running SocketIO Server...')
    socketio.run(
        app,
        host='0.0.0.0',
        port=FLASK_PORT,
        debug=True,
        allow_unsafe_werkzeug=True,
        use_reloader=False,  # Disable reloader to prevent multiple threads
    )

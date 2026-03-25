import socket
import random
import time


def generate_udp_packet():
    # Format: tag_mac;anchor_mac1;distance1;anchor_mac2;distance2;...

    # Using simpler MAC format to match the parsing code in app.py
    tag_mac_adr = "F54D8F000001"
    anchor_mac_adrs = ["b0a732ab1994", "34987a721650", "34987a72a370", "34987a744c1c"]

    # Generate random distances (in mm, realistic for UWB ranging)
    distances = [str(random.randint(10, 700)) for _ in range(len(anchor_mac_adrs))]

    # Build the packet string: tag_mac;anchor1_mac;distance1;anchor2_mac;distance2;...
    parts = [tag_mac_adr]
    for anchor_mac, distance in zip(anchor_mac_adrs, distances):
        parts.append(anchor_mac)
        parts.append(distance)

    packet = ";".join(parts)
    return packet


def main():
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    server_address = ('localhost', 8000)
    print(f"Sending UDP packets to {server_address[0]}:{server_address[1]}")

    try:
        while True:
            # Generate packet
            packet = generate_udp_packet()

            # Send data
            print(f"Sending: {packet}")
            sock.sendto(packet.encode('utf-8'), server_address)

            # Wait a bit before sending the next packet
            time.sleep(0.5)

    except KeyboardInterrupt:
        print("KeyboardInterrupt: Exiting the loop.")
    finally:
        sock.close()
        print("Socket closed")


if __name__ == "__main__":
    main()

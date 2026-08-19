import can
import sys

CAN_INTERFACE = 'socketcan'
CAN_CHANNEL = 'can0' # name of the can interface, must be initialised with CLI before
BITRATE = 500000            # 500 kbps

MSG_TYPE_MASK      = 0x03
MSG_TYPE_REQUEST   = 0x01  # 01: request to the server
MSG_TYPE_OK        = 0x02  # 10: OK response
MSG_TYPE_NOT_OK    = 0x03  # 11: NOT_OK response

# ---AUTHORIZED CARD DIRECTORY (Door IDs in Hexadecimal) ---
# The keys are the isic id and the values are the autorized doors ids for this card
# it's like a withe list
# simulate a LDAP service
CARD_DIRECTORY = {
    "01020304050607": [0x123, 0x00A, 0x1F0],
    "11223344556677": [0x123],
    "A1B2C3D4E5F600": [0x00A],
}

def start_can_server():
    try:
        bus = can.interface.Bus(channel=CAN_CHANNEL, bustype=CAN_INTERFACE, bitrate=BITRATE)
        print(f"CAN Server started on {CAN_CHANNEL} ({BITRATE // 1000} kbps) Listening...")
    except Exception as e:
        print(f"Error during CAN init : {e}")
        sys.exit(1)

    while True:
        msg = bus.recv()
        if msg is None or len(msg.data) < 8:
            continue

        door_id = msg.arbitration_id       # ID CAN (ex: 0x123)
        first_byte = msg.data[0]

        # Extraction of bits [7:6] and right shift
        msg_type = (first_byte >> 6) & MSG_TYPE_MASK

        print(f"door id : {door_id:03X}, msg_type : {msg_type}, msg : {msg.data}")

        if msg_type == MSG_TYPE_REQUEST:
            isic_bytes = msg.data[1:8]
            isic_id_hex = isic_bytes.hex().upper()

            print(f"[REQUEST] Door ID: 0x{door_id:03X} | ISIC ID: {isic_id_hex}")

            authorized_doors = CARD_DIRECTORY.get(isic_id_hex, [])

            if door_id in authorized_doors:
                response_type = MSG_TYPE_OK
                print(f" -> Resolved : ACCESS GRANTED (OK)")
            else:
                response_type = MSG_TYPE_NOT_OK
                print(f" -> Resolved : ACCESS DENIED (NOT_OK)")

            resp_first_byte = (first_byte & ~MSG_TYPE_MASK) | response_type
            resp_payload = bytes([resp_first_byte]) + isic_bytes

            response_msg = can.Message(
                arbitration_id=door_id,
                data=resp_payload,
                is_extended_id=msg.is_extended_id
            )

            try:
                bus.send(response_msg)
                status_str = "OK (10)" if response_type == MSG_TYPE_OK else "NOT_OK (11)"
                print(f"[RESPONSE SEND] DOOR ID: 0x{door_id:03X} | Status: {status_str}\n")
            except can.CanError as e:
                print(f"CAN send error: {e}\n")

if __name__ == "__main__":
    start_can_server()

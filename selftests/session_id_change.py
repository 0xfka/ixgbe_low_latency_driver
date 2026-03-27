import struct
import time
from scapy.all import Ether, IP, UDP, sendp
from seqnum_order_drop import ordered_pkt

ordered_pkt(10)

current_ns = int(time.time() * 1e9)


iex_tp_header = struct.pack(
'<BBHIIHHQQQ',
1, # Version
0, # (Reserved)
0x8003, # Message Protocol ID (0x8003 for TOPS)
123, # Channel ID
54321, # Session ID
12, # Payload Length
1, # Message Count
0, # Stream Offset
10, # First Message Sequence Number
current_ns # Send Time
) 
msg_length = struct.pack('<H', 10)
system_event_msg = struct.pack(
'<ccQ',
b'S',        # Message Type
b'O',        # System Event
current_ns   # Timestamp
)
payload = iex_tp_header + msg_length + system_event_msg

packet = Ether(src="a0:88:c2:ac:b9:05", dst="01:00:5e:57:15:03") / IP(src="23.226.155.131", dst="233.215.21.3") / UDP(sport=10377, dport=10377) / payload

sendp(packet, iface="eth2")

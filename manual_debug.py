import socket

HOST = "chatelaine.cylabacademy.net"
PORT = 12087  # Use your current port

with open("target/deploy/solve.so", "rb") as f:
    data = f.read()

print(f"Sending {len(data)} bytes...")
s = socket.socket()
s.connect((HOST, PORT))
# The server first asks for the length
s.sendall(str(len(data)).encode() + b"\n")
# Then it expects the raw bytes
s.sendall(data)
# Read whatever the server sends back
response = s.recv(4096)
print("Response:", response)
s.close()

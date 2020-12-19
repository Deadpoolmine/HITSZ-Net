import socket
import config

client = socket.socket()
client.connect(config.server_ip, config.server_port)

while True:
    message = input("contents: ")
    client.send(message)
    if message == "bye":
        break

client.close()

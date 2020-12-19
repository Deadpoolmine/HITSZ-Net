import socket
import config

server = socket.socket()
server.bind((config.server_ip,config.server_port))
server.listen()

conn, addr = server.accept()

while True:
    info = conn.recv(1024).decode("utf-8")
    print(info)
    if info == "bye":
        break

conn.close()
server.close()
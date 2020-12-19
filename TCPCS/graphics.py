import eel
import socket
from utils import Utils

client = socket.socket()


class Graphics(object):
    @eel.expose
    def __init__(self, conn: socket.socket):
        print("start gui...")
        self.client = conn
        global client
        client = conn
        eel.init("gui")
        try:
            eel.start('index.html')
        except (SystemExit, MemoryError, KeyboardInterrupt):
            # We can do something here if needed
            # But if we don't catch these safely, the script will crash
            print("closing gui...")

    @eel.expose
    def client_send_file(self, path):
        print("start sending file %s", path)
        global client
        Utils.send_file(client, path)

    @eel.expose
    def client_recv_file(self):
        print("start receiving file ")
        global client
        Utils.recv_file("client_mock_data_base", client)

    @eel.expose
    def client_send_message(self, message):
        print("start sending message %s" % message)
        global client
        Utils.send_message(client, message)

    @eel.expose
    def client_recv_message(self):
        global client
        message = Utils.recv_message(client)
        print("received message %s" % message)
        return message

    @eel.expose
    def basic_func(self):
        print("hello eel")

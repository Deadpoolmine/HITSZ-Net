import socket
import threading
from shell import PRIMITIVE, Shell, OPERATION
from utils import Utils
from graphics import Graphics

lock = threading.Lock()


class Client(object):
    def __init__(self):
        self.server_ip = "127.0.0.1"
        self.server_port = 60000
        self.client = socket.socket()
        self.client.connect((self.server_ip, self.server_port))
        self.shell = Shell()
        self.is_gui_on = False
        welcomes = Utils.recv_message(self.client)
        print(welcomes)

    def graphic_ui(self):
        Graphics(self.client)
        self.is_gui_on = False

    def receive(self):
        while True:
            op_type = Utils.recv_message(self.client)
            if op_type == PRIMITIVE.SEND_INFO.name:
                recv_msg = Utils.recv_message(self.client)
                lock.acquire()
                print("receive from server %s: %s" % (self.server_ip, recv_msg))
                lock.release()
                break
            elif op_type == PRIMITIVE.SEND_FILE.name:
                lock.acquire()
                Utils.recv_file("client_mock_data_base", self.client)
                lock.release()
                break
            elif op_type == PRIMITIVE.CHEK_LIST.name:
                lock.acquire()
                directory_str = Utils.recv_message(self.client)
                print("files on server %s:" % self.server_ip)
                directory = directory_str.split(" ")
                for dirent in directory:
                    print("-    " + dirent)
                print("total %d items found" % len(directory))
                lock.release()
                break

    def start(self):
        gui_thread = None
        while True:
            lock.acquire()
            op_type, args = self.shell.read_cmd()
            lock.release()
            if op_type is None:
                continue

            if op_type == OPERATION.BOOT_GRAPHICS:
                gui_thread = threading.Thread(target=self.graphic_ui, args=(), daemon=True)
                gui_thread.start()
                self.is_gui_on = True
                break

            recv_thread = threading.Thread(target=self.receive, args=(), daemon=True)
            recv_thread.start()
            # 客户端发送操作类型，默认为42个字节构成
            # self.client.send(str(op_type.name).encode("utf-8"))
            Utils.send_message(self.client, op_type.name)
            Utils.send_message(self.client, " ".join(args))
            if op_type == PRIMITIVE.SEND_FILE:
                Utils.send_file(self.client, args[0])
            elif op_type == PRIMITIVE.SEND_INFO:
                info = " ".join(args)
                Utils.send_message(self.client, info)
                if info.lower().find("exit") != -1:
                    break
            # time.sleep(1)
            recv_thread.join()
        if self.is_gui_on:
            gui_thread.join()
            print("restart console...")
            self.start()
        self.client.close()


if __name__ == "__main__":
    client = Client()
    client.start()

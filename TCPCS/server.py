from shell import PRIMITIVE
import socket
import threading
import os
from utils import Utils

lock = threading.Lock()


class Server(object):
    def __init__(self):
        server_ip = "192.168.220.131"
        server_port = 60000

        self.server = socket.socket()
        self.server.bind((server_ip, server_port))
        self.server.listen()

        self.threading_pool = []

    def handler(self, conn: socket.socket, index: int):
        Utils.send_message(conn, "Welcome to Deadpool and his star File Sharing Server! Enjoy yourself")
        while True:
            try:
                op_type = Utils.recv_message(conn)
                args = (Utils.recv_message(conn)).split(" ")
                # content_size = struct.unpack('i', conn.recv(4))[0]
                current_thread = threading.currentThread()
                lock.acquire()
                print("current thread id: %d" % current_thread.ident)
                print("request from client %s: operation %s, args %s" % (conn.getpeername(), op_type, args))
                lock.release()

                if op_type == PRIMITIVE.SEND_FILE.name:
                    lock.acquire()
                    Utils.recv_file("server_mock_data_base", conn)
                    lock.release()
                    Utils.send_message(conn, PRIMITIVE.SEND_INFO.name)
                    Utils.send_message(conn, "upload successfully!")
                elif op_type == PRIMITIVE.RECV_FILE.name:
                    path = os.path.join("server_mock_data_base", args[0])
                    if os.path.isfile(path):
                        Utils.send_message(conn, PRIMITIVE.SEND_FILE.name)
                        Utils.send_file(conn, path)
                    else:
                        Utils.send_message(conn, PRIMITIVE.SEND_INFO.name)
                        Utils.send_message(conn, path + " not found!")
                elif op_type == PRIMITIVE.CHEK_LIST.name:
                    path = os.path.abspath("server_mock_data_base")
                    directory = os.listdir(path)
                    print(directory)
                    Utils.send_message(conn, PRIMITIVE.CHEK_LIST.name)
                    Utils.send_message(conn, " ".join(directory))
                else:
                    info = Utils.recv_message(conn)
                    print(info)
                    Utils.send_message(conn, PRIMITIVE.SEND_INFO.name)
                    Utils.send_message(conn, "send "+info + " successfully!")
                    if info == "bye":
                        break
            except socket.error as e:
                print(e)
                break
        # self.threading_pool[index].join()
        self.threading_pool.remove(threading.currentThread())
        return

    def start_server(self):
        while True:
            conn, addr = self.server.accept()
            thread = threading.Thread(target=self.handler, args=(conn, len(self.threading_pool),), daemon=True)
            self.threading_pool.append(thread)
            thread.start()

    def shut_down_server(self):
        self.server.close()
        return


if __name__ == "__main__":
    server = Server()
    server.start_server()

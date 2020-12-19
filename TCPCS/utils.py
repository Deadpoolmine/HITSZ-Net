import socket
import os
import json
import struct
import global_var

class Utils(object):

    @staticmethod
    def send_message(conn: socket.socket, message: str):
        """
        :param conn: connection
        :param message: 要发送的字符，长度必须小于1024
        :return:失败返回-1，成功返回0
        """
        message_byte = message.encode("utf-8")
        if len(message_byte) > global_var.READ_CHUNK:
            return -1
        conn.send(struct.pack("i", len(message_byte)))
        conn.send(message_byte)
        return 0

    @staticmethod
    def recv_message(conn: socket.socket):
        """
        :param conn:connection
        :return: 收到的消息内容，以utf-8编码
        """
        message_header_struct = conn.recv(4)
        message_len = struct.unpack("i", message_header_struct)[0]
        message = conn.recv(message_len).decode("utf-8")
        return message

    @staticmethod
    def recv_file(directory: str, conn: socket.socket):
        """
        :param directory: 指定本机目录
        :param conn: connection
        :return: null
        """
        file_header_len_struct = conn.recv(4)
        file_header_len = struct.unpack("i", file_header_len_struct)[0]
        file_header_json = conn.recv(file_header_len).decode("utf-8")
        print(file_header_json)
        file_header = json.loads(file_header_json)
        file_name = file_header["file_name"]
        file_size = file_header["file_size"]
        sz_left = file_size
        path = os.path.abspath(os.path.join(directory, file_name))
        with open(path, 'wb') as f:
            while sz_left != 0:
                if sz_left > global_var.READ_CHUNK:
                    line = conn.recv(global_var.READ_CHUNK)
                    # print("recv line %s, len %d" % (line, len(line)))
                    f.write(line)
                    sz_left -= len(line)
                else:
                    line = conn.recv(sz_left)
                    # print("recv line %s, len %d" % (line, len(line)))
                    f.write(line)
                    sz_left -= len(line)

    @staticmethod
    def send_file(conn: socket.socket, path):
        path = os.path.abspath(path)
        file_name = os.path.basename(path)
        file_size = os.path.getsize(path)
        file_header = {
            "file_name": file_name,
            "file_size": file_size
        }
        file_header_byte = json.dumps(file_header).encode("utf-8")
        conn.send(struct.pack("i", len(file_header_byte)))
        conn.send(file_header_byte)
        with open(path, 'rb') as f:
            for line in f:
                conn.send(line)

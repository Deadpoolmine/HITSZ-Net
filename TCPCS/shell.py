from enum import Enum


class PRIMITIVE(Enum):
    SEND_FILE = 1  # PEERS
    RECV_FILE = 2  # PEERS
    SEND_INFO = 3  # PEERS
    CHEK_LIST = 4  # CLIENT发起查看远程服务器列表请求

class OPERATION(Enum):
    BOOT_GRAPHICS = 1
    CLOS_GRAPHICS = 2


class Parser(object):
    def __init__(self):
        pass

    def parse(self, str: str):
        if not len(str):
            return None, None
        str = str.lower().strip()
        op_type = PRIMITIVE.SEND_INFO
        cmd = str.split()
        op = cmd[0]
        args = cmd[1:]
        if op == "upload" or op == "up":
            op_type = PRIMITIVE.SEND_FILE
        elif op == "download" or op == "down":
            op_type = PRIMITIVE.RECV_FILE
        elif op == "ls" or op == "list":
            op_type = PRIMITIVE.CHEK_LIST
        elif op == "gui" or op == "ui":
            op_type = OPERATION.BOOT_GRAPHICS
        if op_type == PRIMITIVE.SEND_INFO:
            args = cmd[0:]
        return op_type, args


class Shell(object):
    def __init__(self):
        print("start shell...")
        self.__parser = Parser()

    def read_cmd(self):
        """
        return op_type , args
        """
        str = input("deadpool&star >>> ")
        return self.__parser.parse(str)

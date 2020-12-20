import os
import re
import socket
import base64
from enum import Enum
import json
from bs4 import BeautifulSoup


class SMTPOP(Enum):
    HELO = 1,
    AUTH = 2,
    VRIFY = 3,
    FROM_MAIL = 4,
    TO_MAIL = 5,
    DATA = 6,
    DATA_BODY = 7
    QUIT = 8


class SMTPClient(object):
    def __init__(self, user_name, pass_word, receiver):
        self.qq_mail_server_smtp = "smtp.qq.com"
        self.qq_mail_server_smtp_port = 587  # or 465
        self.qq_mail_server_pop3 = "pop.qq.com"
        self.qq_mail_server_pop3_port = 995
        self.google_mail_server_smtp = "smtp.gmail.com"
        self.google_mail_server_smtp_port = 587

        self.end_msg = "\r\n.\r\n"
        self.boundary = "--BOUNDARY\r\n"
        print("Welcome to Deadpool and Star Main Client. Have Fun!")

        # Sender Receiver
        self.sender = user_name  # "2841954631@qq.com"
        self.receiver = receiver  # "1009274113@qq.com"

        # Auth
        # 用户名
        self.user_name = str(base64.b64encode(user_name.encode('utf-8')), encoding="utf-8")
        # 授权码，根据qq邮箱官方指导，记得要转为base64
        self.pass_word = str(base64.b64encode(pass_word.encode("utf-8")), encoding="utf-8")

        # Create Socket
        self.client_socket = socket.socket()
        self.client_socket.connect((self.qq_mail_server_smtp, self.qq_mail_server_smtp_port))
        # self.client_socket.connect((self.google_mail_server_smtp, self.google_mail_server_smtp_port))
        recv = self.client_socket.recv(1024).decode("utf-8")
        print("tcp connect: " + recv)
        if recv[:3] != '220':
            print("220 reply not received from server")
        else:
            print("%s server ready!" % self.qq_mail_server_smtp)

    def attach_image(self, path: str, id: str):
        client_socket = self.client_socket
        file_ext = os.path.splitext(path)[-1]
        file_name = os.path.basename(path)
        file_type = "image/jpg"
        if file_ext == ".jpg":
            file_type = "image/jpg"
        content = ""
        content += "\r\n\r\n" + self.boundary
        content += "Content-type:" + file_type + ";\r\n"
        content += "Content-ID:<" + id + ">\r\n"  # file ID
        content += "Content-Disposition: attachment; filename='" + file_name + "'\r\n"
        content += "Content-Transfer-Encoding:base64\r\n\r\n"
        content += "\r\n"
        client_socket.send(content.encode())
        fb = open(path, "rb")
        while True:
            data = fb.read(1024)
            if not data:
                break
            client_socket.send(base64.b64encode(data))
        fb.close()

    def parse_content(self, content: str):
        soup = BeautifulSoup(content, 'html.parser')
        pic_list = soup.find_all("img")
        cids = []
        # 找出img标签里后面的链接，并写进列表里
        for pic in pic_list:
            url = pic.get('src')
            cids.append(url)
        return cids

    def base_send_receive(self, smtp_op: SMTPOP, msg: str):
        client_socket = self.client_socket
        command = ""
        if smtp_op == SMTPOP.HELO:
            command = "HELO " + msg + "\r\n"
        elif smtp_op == SMTPOP.AUTH:
            command = "AUTH login " + self.user_name + "\r\n"
        elif smtp_op == SMTPOP.VRIFY:
            command = self.pass_word + "\r\n"
        elif smtp_op == SMTPOP.FROM_MAIL:
            command = "MAIL FROM:<" + msg + ">\r\n"
        elif smtp_op == SMTPOP.TO_MAIL:
            command = "RCPT TO:<" + msg + ">\r\n"
        elif smtp_op == SMTPOP.DATA:
            command = "DATA \r\n"
        elif smtp_op == SMTPOP.DATA_BODY:
            msg_content = json.loads(msg)
            subject = msg_content["subject"]
            content = msg_content["content"]
            command += "from:<" + self.sender + ">\r\n"
            command += "to:<" + self.receiver + ">\r\n"
            command += "subject:" + subject + "\r\n"
            # 如果要在邮件中要添加附件，就必须将整封邮件的MIME类型定义为multipart/mixed
            command += "Content-Type:multipart/mixed;boundary='BOUNDARY'\r\n\r\n"
            command += "\r\n\r\n" + self.boundary
            command += "Content-Type:text/html;boundary='BOUNDARY'\r\n\r\n"
            command += content
            client_socket.send(command.encode())
            cids = self.parse_content(content)
            i = 0
            for cid in cids:
                i += 1
                # import win32ui  # 引入模块
                # dlg = win32ui.CreateFileDialog(1)  # 参数 1 表示打开文件对话框
                # dlg.SetOFNInitialDir('E:\Mess\MyLoveImage')  # 设置打开文件对话框中的初始显示目录
                # dlg.DoModal()
                # filename = dlg.GetPathName()  # 返回选择的文件路径和名称
                file_path = input("文件"+ str(i) +"路径：")
                id = str(cid).split(":")[1]
                self.attach_image(file_path, id)
            # self.attach_image("./deadpool.jpg", "red")
            command = self.end_msg
        elif smtp_op == SMTPOP.QUIT:
            command = "QUIT \r\n"
        print(command)
        client_socket.send((command.encode()))
        recv = client_socket.recv(1024).decode()
        if smtp_op == SMTPOP.HELO:
            if recv[:3] != '250':
                print("Hello: 250 reply not received from server")
        elif smtp_op == SMTPOP.AUTH:
            if recv[:3] != '334':
                print("Authority: 334 reply not received from server")
        elif smtp_op == SMTPOP.VRIFY:
            if recv[:3] != '235':
                print("Verify:235 reply not received from server")
        elif smtp_op == SMTPOP.FROM_MAIL:
            if recv[:3] != '250':
                print("From:<%s> is not found" % self.sender)
        elif smtp_op == SMTPOP.TO_MAIL:
            if recv[:3] != '250':
                print("To:<%s> is not found" % self.receiver)
        elif smtp_op == SMTPOP.DATA:
            print("Data: " + recv)
        elif smtp_op == SMTPOP.DATA_BODY:
            print("Message: " + recv)
        elif smtp_op == SMTPOP.QUIT:
            print("Quit: " + recv)
            client_socket.close()
            print("Good Bye!")

    def login(self, user_name, pass_word):
        self.base_send_receive(SMTPOP.HELO, user_name)
        self.base_send_receive(SMTPOP.AUTH, "")
        self.base_send_receive(SMTPOP.VRIFY, "")
        print("login success!")

    def send(self, msg):
        self.base_send_receive(SMTPOP.FROM_MAIL, self.sender)
        self.base_send_receive(SMTPOP.TO_MAIL, self.receiver)
        self.base_send_receive(SMTPOP.DATA, "")
        self.base_send_receive(SMTPOP.DATA_BODY, msg)
        self.base_send_receive(SMTPOP.QUIT, "")


if __name__ == '__main__':
    pass_word = "bedeoyjagwgpdhai" # qq:"bedeoyjagwgpdhai"
    user_name = "2841954631@qq.com" # qq: "2841954631@qq.com"
    receiver = "1009274113@qq.com"  # "1009274113@qq.com" 13689524996@163.com
    # content = <img src="cid:yellow"> <img src="cid:red">
    # user_name = input("Your User Name(xx@xx.com)：")
    # pass_word = input("Your Pass Word(Authority Code): ")
    # receiver = input("The one You Want to Send(xx@xx.com): ")
    mail_client = SMTPClient(user_name, pass_word, receiver)
    mail_client.login(user_name, pass_word)
    subject = input("Input Mail Subject: ")
    content = input("Input Mail Content: ")
    msg = {
        "subject": subject,
        "content": content
    }
    msg_str = json.dumps(msg)
    mail_client.send(msg_str)

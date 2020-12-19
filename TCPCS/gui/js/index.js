const SEND_FILE = "SEND_FILE";
const RECV_FILE = "RECV_FILE";
const SEND_INFO = "SEND_INFO";
const CHEK_LIST = "CHEK_LIST";
const self = "mock_self, we don't need that";



function upload() {
    let picker = document.getElementById("file_picker");
    picker.click()
    console.log(picker)
    let file_name = null
    picker.addEventListener("change", function (e) {
        e.stopPropagation();
        e.stopImmediatePropagation();
        const file_list = this.files;
        if(file_name == null){
            console.log(file_list)
            alert("Upload Start...");
            eel.client_send_message(self, SEND_FILE);
            eel.client_send_message(self, file_list[0].name);
            eel.client_send_file(self, file_list[0].name)(function (){
                //alert("Upload Over!");
                alert(file_list[0].name)
                file_name = null;
            })
            //保证把回复信息全部读走
            setTimeout(function (){
                eel.client_recv_message(self);
                eel.client_recv_message(self)(function (message) {
                    alert(message);
                })
            }, 100);
        }
    }, false);
}

function download() {

    //操作
    eel.client_send_message(self, CHEK_LIST);
    //参数
    eel.client_send_message(self, "");
    //结果
    eel.client_recv_message(self)(function (op_type) {
        if(op_type === CHEK_LIST){
            eel.client_recv_message(self)(function (file_list) {
                console.log(file_list);
                var ul = document.querySelector("ul");
                ul.innerHTML = "";
                var file_list = file_list.split(' ');
                for (let i = 0; i < file_list.length; i++) {
                    let file_name = file_list[i];
                    var listItem = document.createElement("li");
                    var img = document.createElement("img");
                    img.src = "../sources/python_logo.png";
                    var title = document.createElement("h3");
                    title.textContent = file_name;
                    var desc = document.createElement("p");
                    desc.textContent = "这是一个Python文件";
                    listItem.appendChild(img);
                    listItem.appendChild(title);
                    listItem.appendChild(desc);
                    listItem.addEventListener("click", function () {
                        alert("Download start...");
                        eel.client_send_message(self, RECV_FILE);
                        eel.client_send_message(self, file_name);
                        setTimeout(function (){
                            eel.client_recv_message(self)(function (op_type) {
                                if(op_type === SEND_FILE){
                                    eel.client_recv_file(self)(function (){
                                        alert("Download Finish!");
                                    })
                                }
                            })
                        }, 100);
                    })
                    ul.appendChild(listItem);
                }
            })
        }
    })
}
# CloudClipboard
基于muduo实现的云剪贴板，实现跨设备cv 

先注册一个账号，登录同一账号的设备之间可以互相复制粘贴。

一台设备只需登录一次，第二次以后会自动登录。

支持文本/文件/文件夹复制，但是尽量不要复制大的文件或者文件夹，可能会卡死客户端 

客户端qt虽然跨平台，但是在一些剪贴板操作上没有完全平台无关，

比如剪贴板的mime格式标识在win和linux下是不一样的，所以提供了win和linux两个客户端版本

#### 跨设备复制文本
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard1.png)
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard2.png)

#### 跨设备复制文件
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard3.png)
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard4.png)

#### 跨设备复制文件夹
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard5.png)
![](https://github.com/JustDoIt0910/MarkDownPictures/blob/main/clipboard6.png)

### Usage
#### server
依赖muduo，需要链接muduo静态库
```bash
cd server
cmake .
make
stdbuf -i0 -o0 -e0 nohup ./server &
```

#### client
使用qt开发，win版本用mingw编译套件

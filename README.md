# CloudClipboard
基于muduo实现的云剪贴板，实现跨设备cv 

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
```

#### client
使用qt开发，win版本用mingw编译套件

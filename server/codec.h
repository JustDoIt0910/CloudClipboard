//
// Created by zr on 22-12-23.
//

#ifndef __CLIPSERVER_CODEC_H__
#define __CLIPSERVER_CODEC_H__

#include "muduo/net/TcpConnection.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/Endian.h"

#include <string>
#include <functional>
#include <fstream>

struct Auth
{
    std::string user;
    std::string pwd;

    Auth(){}

    Auth(const std::string& u, const std::string& p):
        user(u), pwd(p){}

    void writeToFile(std::ofstream& ofs) const
    {
        ofs << user << ":" << pwd << '\n';
    }
};

typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                            const Auth&, bool reg, muduo::Timestamp)> AuthMessageCallback;

typedef std::function<void (const muduo::net::TcpConnectionPtr&,
                            const std::string& message,
                            muduo::Timestamp)> StringMessageCallback;

class Codec {

public:
    Codec(const AuthMessageCallback& authMsgCb, const StringMessageCallback& strMsgCb):
        authMessageCallback(authMsgCb), stringMessageCallback(strMsgCb)
    {}

//    +------+----+----+----------+----------+
//    | 0x0  |len1|len2| username | password |  register
//    +------+----+----+----------+----------+
//        1     1    1      var        var
//    +------+----+----+----------+----------+
//    | 0x1  |len1|len2| username | password |  login
//    +------+----+----+----------+----------+
//        1     1    1      var        var
//    +------+----+--------+---------------------+
//    | 0x2  |mime|  len   |         data        |  data
//    +------+----+--------+---------------------+
//        1     1      2             var
//    +------+-------+
//    | 0x3  |  code | success
//    +------+-------+
//    +------+-------+
//    | 0x4  |  code | fail
//    +------+-------+
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                   muduo::net::Buffer* buf,
                   muduo::Timestamp receiveTime)
    {
        if(buf->readableBytes() >= minHeaderLen)
        {
            const char* header = buf->peek();
            char op = *header;
            if(op == OP_REG || op == OP_LOGIN)
            {
                uint16_t len = muduo::net::sockets::networkToHost16(*(const uint16_t*)(header + 1));
                char len1 = (len >> 8) & 0xFF;
                char len2 = len & 0xFF;
                if(buf->readableBytes() >= 3 + len1 + len2)
                {
                    buf->retrieve(3);
                    std::string user(buf->peek(), len1);
                    std::string pwd(buf->peek() + len1, len2);
                    bool reg = op == OP_REG;
                    authMessageCallback(conn, Auth(user, pwd), reg, receiveTime);
                    buf->retrieve(len1 + len2);
                }
            }
            else if(op == OP_DATA && buf->readableBytes() >= 4)
            {
                char mimeType = *(header + 1);
                uint16_t len = muduo::net::sockets::networkToHost16(*(const uint16_t*)(header + 2));
                if(buf->readableBytes() >= 4 + len)
                {
                    buf->retrieve(4);
                    if(mimeType == MIME_TEXT)
                    {
                        std::string data(buf->peek(), len);
                        stringMessageCallback(conn, data, receiveTime);
                    }
                    buf->retrieve(len);
                }
            }
            else
                LOG_WARN << "unknown op code [" << op << "]";
        }
    }

    void sendResp(const muduo::net::TcpConnectionPtr& conn, bool success, char op, char code)
    {
        muduo::net::Buffer buf;
        if(success)
        {
            char tmp[] = {3, op, code};
            buf.append(tmp, 3);
        }
        else
        {
            char tmp[] = {4, op, code};
            buf.append(tmp, 3);
        }
        conn->send(&buf);
    }

    void sendText(const muduo::net::TcpConnectionPtr& conn, const std::string& msg)
    {
        muduo::net::Buffer buf;
        uint16_t len = msg.length();
        char tmp[] = {2, MIME_TEXT};
        buf.append(tmp, 2);
        buf.appendInt16(static_cast<int16_t>(len));
        buf.append(msg.c_str(), msg.length());
        conn->send(&buf);
    }

    enum ErrCode
    {
        OK = 0,
        ERR_USERNAME_EXIST = -1,
        ERR_USER_NOT_FOUND = -2,
        ERR_WRONG_PASSWORD = -3
    };

    enum OpCode
    {
        OP_REG,
        OP_LOGIN,
        OP_DATA
    };

    enum MimeType
    {
        MIME_TEXT,
        MIME_FILE
    };

private:
    const static int minHeaderLen = 3;
    AuthMessageCallback authMessageCallback;
    StringMessageCallback stringMessageCallback;
};

#endif


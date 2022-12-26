//
// Created by zr on 22-12-23.
//

#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Logging.h"
#include "codec.h"

#include <unordered_map>
#include <unordered_set>
#include <string.h>

using namespace muduo;
using std::placeholders::_4;

class ClipServer: public muduo::noncopyable
{
public:
    ClipServer(net::EventLoop* loop, const net::InetAddress& listenAddr):
        server(loop, listenAddr, "ClipServer"),
        codec(std::bind(&ClipServer::onAuthMessage, this, _1, _2, _3, _4),
              std::bind(&ClipServer::onStringMessage, this, _1, _2, _3),
              std::bind(&ClipServer::onHeartbeat, this, _1, _2))
    {
        server.setConnectionCallback(
                std::bind(&ClipServer::onConnection, this, _1));
        server.setMessageCallback(std::bind(&Codec::onMessage, &codec, _1, _2, _3));
        server.setThreadNum(4);
    }

    void start()
    {
        server.start();
        //TODO start timer
    }

private:
    void onConnection(const net::TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "up": "down");

        if(!conn->connected())
        {
            MutexLockGuard lock(mutex);
            auto it = addrToNamespace.find(conn->peerAddress().toIpPort());
            if(it != addrToNamespace.end())
            {
                LOG_INFO << "client [" << conn->peerAddress().toIpPort() << "] logout";
                ConnectionList& conns = namespaceToConns[it->second];
                conns.erase(conn);
                if(conns.size() == 0)
                {
                    namespaceToConns.erase(it->second);
                    addrToNamespace.erase(it);
                }
            }
        }
    }

    void onAuthMessage(const muduo::net::TcpConnectionPtr& conn, const Auth& auth,
                       bool reg, muduo::Timestamp receiveTime)
    {
        std::ifstream ifs("users", std::ios::in);
        if(!ifs.is_open())
            LOG_FATAL << "can not open users file";
        if(reg)
        {
            LOG_INFO << "user(" << auth.user << ", " << auth.pwd << ") register";
            Auth i;
            if(!loadUser(ifs, auth.user, i))
            {
                std::ofstream ofs("users", std::ios::app);
                auth.writeToFile(ofs);
                ofs.close();
                codec.sendResp(conn, true, Codec::OP_REG, Codec::OK);
            }
            else
            {
                LOG_INFO << "username " << auth.user << "already exists";
                codec.sendResp(conn, false, Codec::OP_REG, Codec::ERR_USERNAME_EXIST);
            }
        }
        else
        {
            LOG_INFO << "users (" << auth.user << ", " << auth.pwd << ") login";
            Auth stored;
            if(loadUser(ifs, auth.user, stored))
            {
                if(auth.pwd == stored.pwd)
                {
                    LOG_INFO << "user " << auth.user << " login success";
                    MutexLockGuard lock(mutex);
                    assert(addrToNamespace.find(conn->peerAddress().toIpPort()) == addrToNamespace.end());
                    addrToNamespace[conn->peerAddress().toIpPort()] = auth.user;
                    namespaceToConns[auth.user].insert(conn);
                    codec.sendResp(conn, true, Codec::OP_LOGIN, Codec::OK);
                }
                else
                {
                    LOG_INFO << "user " << auth.user << " login fail, wrong password";
                    codec.sendResp(conn, false, Codec::OP_LOGIN, Codec::ERR_WRONG_PASSWORD);
                }
            }
            else
            {
                LOG_INFO << "username " << auth.user << " doesn't exist";
                codec.sendResp(conn, false, Codec::OP_LOGIN, Codec::ERR_USER_NOT_FOUND);
            }

        }
        ifs.close();
    }

    bool loadUser(std::ifstream& ifs, const std::string username, Auth& auth)
    {
        char buf[100];
        bzero(buf, sizeof(buf));
        while(ifs.getline(buf, sizeof(buf)))
        {
            char* end = buf + 50;
            char* where = std::find(buf, end, ':');
            assert(where != end);
            std::string user(buf, where - buf);
            if(user == username)
            {
                auth.user = user;
                auth.pwd = where + 1;
                return true;
            }
        }
        return false;
    }

    void onStringMessage(const muduo::net::TcpConnectionPtr& conn, const std::string& message,
                         muduo::Timestamp)
    {
        LOG_INFO << "from [" << conn->peerAddress().toIpPort() << "]: " << message;
        auto it = addrToNamespace.find(conn->peerAddress().toIpPort());
        if(it == addrToNamespace.end())
            return;
        for(const muduo::net::TcpConnectionPtr& c: namespaceToConns[it->second])
        {
            if(c == conn)
                continue;
            LOG_INFO << message << " -> " << c->peerAddress().toIpPort();
            codec.sendText(c, message);
        }
    }

    void onHeartbeat(const muduo::net::TcpConnectionPtr& conn, muduo::Timestamp)
    {

    }

    typedef std::unordered_set<net::TcpConnectionPtr> ConnectionList;

    std::unordered_map<std::string, ConnectionList> namespaceToConns;
    std::unordered_map<std::string, std::string> addrToNamespace;

    MutexLock mutex;
    Codec codec;
    net::TcpServer server;
};

int main(int argc, char* argv[])
{
    net::EventLoop loop;
    net::InetAddress serverAddr(8090);
    ClipServer server(&loop, serverAddr);
    server.start();
    loop.loop();
    return 0;
}
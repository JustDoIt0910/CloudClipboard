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
              std::bind(&ClipServer::onFileData, this, _1, _2, _3),
              std::bind(&ClipServer::onDirInfo, this, _1, _2, _3),
              std::bind(&ClipServer::onHeartbeat, this, _1, _2)),
        connections(new ConnectionMap),
        namespaceToAddrs(new NamespaceAddrMap)
    {
        server.setConnectionCallback(
                std::bind(&ClipServer::onConnection, this, _1));
        server.setMessageCallback(std::bind(&Codec::onMessage, &codec, _1, _2, _3));
        server.setThreadNum(4);
    }

    void start()
    {
        server.start();
        net::EventLoop* loop = server.getLoop();
        loop->runEvery(20, std::bind(&ClipServer::checkAlive, this));
    }

private:
    void onConnection(const net::TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "up": "down");

        if(!conn->connected())
        {
            std::string addrStr = conn->peerAddress().toIpPort();
            MutexLockGuard lock(mutex);
            auto it = connections->find(addrStr);
            if(it != connections->end())
            {
                LOG_INFO << "client [" << addrStr << "] logout";
                if(!namespaceToAddrs.unique())
                    namespaceToAddrs.reset(new NamespaceAddrMap(*namespaceToAddrs));
                if(!connections.unique())
                    connections.reset(new ConnectionMap(*connections));

                (*namespaceToAddrs)[it->second.ns].erase(addrStr);
                if((*namespaceToAddrs)[it->second.ns].size() == 0)
                    namespaceToAddrs->erase(it->second.ns);
                connections->erase(it);
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
                    std::string addrStr = conn->peerAddress().toIpPort();
                    {
                        MutexLockGuard lock(mutex);
                        assert(connections->find(addrStr) == connections->end());
                        if(!namespaceToAddrs.unique())
                            namespaceToAddrs.reset(new NamespaceAddrMap(*namespaceToAddrs));
                        if(!connections.unique())
                            connections.reset(new ConnectionMap(*connections));
                        (*namespaceToAddrs)[auth.user].insert(addrStr);
                        (*connections)[addrStr] = Connection(conn, auth.user);
                    }
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
                         muduo::Timestamp receiveTime)
    {
        LOG_INFO << "from [" << conn->peerAddress().toIpPort() << "]: " << message;
        onPacket(conn, &message, Codec::MIME_TEXT, receiveTime);
    }

    void onFileData(const muduo::net::TcpConnectionPtr& conn, const muduo::net::Buffer& buf,
                    muduo::Timestamp receiveTime)
    {
        LOG_INFO << "from [" << conn->peerAddress().toIpPort() << "]: FileData " << buf.readableBytes() << " bytes";
        onPacket(conn, &buf, Codec::MIME_FILE, receiveTime);
    }

    void onDirInfo(const muduo::net::TcpConnectionPtr& conn, const std::string dir,
                   muduo::Timestamp receiveTime)
    {
        LOG_INFO << "from [" << conn->peerAddress().toIpPort() << "]: DirInfo :" << dir;
        onPacket(conn, &dir, Codec::MIME_DIR, receiveTime);
    }

    void onPacket(const muduo::net::TcpConnectionPtr& conn, const void* data,
                  Codec::MimeType mime, muduo::Timestamp)
    {
        NamespaceAddrMapPtr namespaceToAddrSnapshot;
        ConnectionMapPtr connectionSnapshot;
        auto it = connections->cbegin();
        {
            MutexLockGuard lock(mutex);
            it = connections->find(conn->peerAddress().toIpPort());
            if(it == connections->end())
                return;
            namespaceToAddrSnapshot = namespaceToAddrs;
            connectionSnapshot = connections;
        }
        for(const std::string& addr: (*namespaceToAddrSnapshot)[it->second.ns])
        {
            if(addr == conn->peerAddress().toIpPort())
                continue;
            if(mime == Codec::MIME_TEXT)
            {
                std::string massage = *static_cast<const std::string*>(data);
                LOG_INFO << massage << " -> " << addr;
                codec.sendText((*connectionSnapshot)[addr].conn, massage);
            }
            else if(mime == Codec::MIME_FILE)
            {
                muduo::net::Buffer buf = *static_cast<const muduo::net::Buffer*>(data);
                LOG_INFO << "FileData[" << buf.readableBytes() << "] -> " << addr;
                codec.sendFile((*connectionSnapshot)[addr].conn, buf);
            }
            else
            {
                std::string dir = *static_cast<const std::string*>(data);
                LOG_INFO << "Dir[" << dir << "] -> " << addr;
                codec.sendDirInfo((*connectionSnapshot)[addr].conn, dir);
            }
        }
    }

    void onHeartbeat(const muduo::net::TcpConnectionPtr& conn, muduo::Timestamp)
    {
        LOG_INFO << "heartbeat from connection[" << conn->peerAddress().toIpPort() << "]";
        {
            MutexLockGuard lock(mutex);
            (*connections)[conn->peerAddress().toIpPort()].ttl = 3;
        }
        codec.sendHeartbeatAck(conn);
    }

    void checkAlive()
    {
        LOG_INFO << "checking alive...";
        MutexLockGuard lock(mutex);
        for(auto it = connections->begin(); it != connections->end(); it++)
        {
            LOG_INFO << "connection [" << it->first << "] ttl = " << it->second.ttl;
            if(it->second.ttl-- <= 0)
            {
                LOG_INFO << "connection [" << it->first << "] offline";
                it->second.conn->forceClose();
            }
        }
    }

    struct Connection
    {
        net::TcpConnectionPtr conn;
        std::string ns;
        int ttl;

        Connection(){}
        Connection(const net::TcpConnectionPtr& _conn, const std::string& _ns):
            conn(_conn), ns(_ns), ttl(10){}
    };

    typedef std::unordered_map<std::string, Connection> ConnectionMap;
    typedef std::shared_ptr<ConnectionMap> ConnectionMapPtr;
    typedef std::unordered_map<std::string, std::unordered_set<std::string>> NamespaceAddrMap;
    typedef std::shared_ptr<NamespaceAddrMap> NamespaceAddrMapPtr;

    NamespaceAddrMapPtr namespaceToAddrs;
    ConnectionMapPtr connections;

    MutexLock mutex;
    Codec codec;
    net::TcpServer server;
};

int main(int argc, char* argv[])
{
    net::EventLoop loop;
    uint16_t port = 8090;
    if(argc > 1)
        port = atoi(argv[1]);
    net::InetAddress serverAddr(port);
    ClipServer server(&loop, serverAddr);
    server.start();
    loop.loop();
    return 0;
}
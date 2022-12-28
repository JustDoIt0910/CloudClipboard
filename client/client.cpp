#include "client.h"
#include <QHostAddress>
#include <QCryptographicHash>
#include <QtEndian>
#include <QThread>
#include <stdio.h>

const static int BUFSIZE = 10 * 1024;

Client::Client(QString _ip, quint16 _port):
    ip(_ip), port(_port), noAck(0)
{
    client = new QTcpSocket();
    timer = new QTimer();
    timer->setInterval(20 * 1000);
    connect(timer, &QTimer::timeout, this, &Client::sendHeartbeat);
    connect(client, &QTcpSocket::readyRead, this, [&](){
        QByteArray resp = client->readAll();
        inputBuf.append(resp);
        while(inputBuf.size() >= 2)
        {
            if(inputBuf.at(0) == OP_HEARTBEAT)
            {
                if(inputBuf.at(1) != HB_ACK)
                    qDebug() << "invalid heartbeat ack";
                else
                    noAck = 0;
                inputBuf.remove(0, 2);
            }
            if(inputBuf.size() >= 3)
            {
                if(inputBuf.at(0) == OP_SUCCESS_RESP || inputBuf.at(0) == OP_FAIL_RESP)
                {
                    if(inputBuf.at(0) == 3)
                    {
                        timer->start();
                        emit success(inputBuf.at(1));
                    }
                    else
                    {
                        client->disconnectFromHost();
                        emit fail(inputBuf.at(1), static_cast<ErrCode>(inputBuf.at(2)));
                    }
                    inputBuf.remove(0, 3);
                }
                else if(inputBuf.at(0) == OP_DATA)
                {
                    if(inputBuf.size() < 4)
                        break;
                    char mime = inputBuf.at(1);
                    uint16_t len = inputBuf.at(2);
                    len = len << 8 | (inputBuf.at(3) & 0xFF);
                    if(inputBuf.size() >= 4 + len)
                    {
                        inputBuf.remove(0, 4);
                        QByteArray data = inputBuf.left(len);
                        if(mime == MIME_TEXT)
                            emit hasTextData(QString(data));
                        else
                            emit hasFileData(data);
                        inputBuf.remove(0, len);
                    }
                    else
                        break;

                }
                else
                    inputBuf.clear();
            }
        }
    });

    connect(client, &QTcpSocket::connected, this, [=](){
        char op = reg ? 0 : 1;
        QString pwd = password + "cdq3kf9dh";
        QByteArray encoded = QCryptographicHash::hash(pwd.toLatin1(), QCryptographicHash::Md5);
        QString encodedPwd = encoded.toHex();

        QByteArray userData = username.toLatin1();
        QByteArray pwdData = encodedPwd.toLatin1();
        QByteArray pkt;
        pkt.append(op);
        uchar buf[2];
        quint16 len = userData.size() << 8 | pwdData.size();
        qToBigEndian(len, buf);
        pkt.append((char*)buf, 2);
        pkt.append(userData);
        pkt.append(pwdData);
        client->write(pkt);
    });
}

void Client::loginOrReg(QString username, QString password, bool reg)
{
    this->username = username;
    this->password = password;
    this->reg = reg;
    client->disconnectFromHost();
    client->connectToHost(QHostAddress(ip), port);
}

void Client::sendHeartbeat()
{
    if(noAck >= 2)
    {
        noAck = 0;
        timer->stop();
        qDebug() << "no heartbeat ack, reconnecting...";
        client->disconnectFromHost();
        reg = false;
        client->connectToHost(QHostAddress(ip), port);
        return;
    }
    QByteArray pkt;
    pkt.append(OP_HEARTBEAT);
    pkt.append(HB_SEND);
    client->write(pkt);
    noAck++;
}

void Client::sendText(QString text)
{
    QByteArray pkt = makeHeader(MIME_TEXT, text.toUtf8().length());
    pkt.append(text);
    client->write(pkt);
}

void Client::sendFile(QString filepath)
{
    FILE* fp = ::fopen(filepath.toStdString().c_str(), "rb");
    if(!fp)
        return;
    QStringList list = filepath.split("/");
    QString filename = list[list.size() - 1];
    QByteArray pkt = makeHeader(MIME_FILE, filename.toUtf8().length());
    pkt.append(filename);
    client->write(pkt);

    char buf[BUFSIZE];
    size_t nread = fread(buf, 1, sizeof(buf), fp);
    while(nread > 0)
    {
        pkt = makeHeader(MIME_FILE, nread);
        pkt.append(buf, nread);
        client->write(pkt);
        nread = fread(buf, 1, sizeof(buf), fp);
    }
    pkt = makeHeader(MIME_FILE, 0);
    client->write(pkt);
    ::fclose(fp);
}

QByteArray Client::makeHeader(MimeType mime, quint16 len)
{
    QByteArray header;
    header.append(OP_DATA);
    header.append((char)mime);
    uchar buf[2];
    qToBigEndian(len, buf);
    header.append((char*)buf, 2);
    return header;
}

Client::~Client()
{
    timer->stop();
    client->disconnectFromHost();
    delete client;
    delete timer;
}

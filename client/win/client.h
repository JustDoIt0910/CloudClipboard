#ifndef CLIENT_H
#define CLIENT_H
#include <QTcpSocket>
#include <QString>
#include <QTimer>
#include <atomic>
#include <QTextCodec>


class Client: public QObject
{
    Q_OBJECT
public:
    Client(QString ip, quint16 port);
    void loginOrReg(QString username, QString password, bool reg);
    void sendText(QString text);
    void sendFile(QString& filepath);
    void sendDir(QString& path);
    ~Client();

    enum MimeType { MIME_TEXT, MIME_FILE, MIME_DIR };
    enum HeartbeatType{ HB_SEND, HB_ACK };
    enum OpCode
    {
        OP_REG,
        OP_LOGIN,
        OP_DATA,
        OP_SUCCESS_RESP,
        OP_FAIL_RESP,
        OP_HEARTBEAT
    };
    enum ErrCode
    {
        OK = 0,
        ERR_USERNAME_EXIST = -1,
        ERR_USER_NOT_FOUND = -2,
        ERR_WRONG_PASSWORD = -3
    };

signals:
    void success(char op);
    void fail(char op, Client::ErrCode code);
    void hasTextData(QString data);
    void hasFileData(QByteArray& data);
    void hasDirInfo(QString dir);

private:
    QString ip;
    quint16 port;
    QByteArray inputBuf;
    QString username, password;
    bool reg;
    QTcpSocket* client;
    QTimer* timer;
    std::atomic_char noAck;
    QTextCodec* codec;
    bool retrying = false;

    void onTimeout();
    void onConnected();
    void decode();
    void _sendDir(QString& base);
    QByteArray makeHeader(MimeType mime, quint16 len);
};

#endif // CLIENT_H

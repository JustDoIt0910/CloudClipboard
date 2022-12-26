#ifndef CLIENT_H
#define CLIENT_H
#include <QTcpSocket>
#include <QString>


class Client: public QObject
{
    Q_OBJECT
public:
    Client(QString ip, quint16 port);
    void loginOrReg(QString username, QString password, bool reg);
    void sendText(QString text);
    ~Client();

    enum MimeType
    {
        MIME_TEXT,
        MIME_FILE
    };
    enum OpCode
    {
        OP_REG,
        OP_LOGIN,
        OP_DATA
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
    void fail(char op, ErrCode code);
    void hasTextData(QString data);

private:
    QString ip;
    quint16 port;
    QByteArray inputBuf;
    QString username, password;
    bool reg;
    QTcpSocket* client;

    QByteArray makeHeader(MimeType mime, quint16 len);
};

#endif // CLIENT_H

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QByteArray>
#include <QString>
class TcpServer:public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);
    bool listen(quint16 port);  //对外接口
    QString errorString() const;
signals:
    void lineReceived(const QByteArray &line);  //切出了一段完整内容
    void clientConnected(const QString &peer);
    void clientDisconnected();
private slots:
    void onNewConnection(); //新连接
    void onReadyRead();     //拆包函数，拆完存到m_rxBuf中
    void onDisconnected();
private:
    QTcpServer *server;
    QTcpSocket *clientSocket;
    QByteArray m_rxBuf; //读取存储区
};

#endif // TCPSERVER_H

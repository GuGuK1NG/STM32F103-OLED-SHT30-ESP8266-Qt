#include "tcpserver.h"
#include <QDebug>
TcpServer::TcpServer(QObject *parent):QObject(parent) {
    server=new QTcpServer(this);
    clientSocket = nullptr;
    connect(server,&QTcpServer::newConnection,this,&TcpServer::onNewConnection);
}

bool TcpServer::listen(quint16 port)
{
    if(!server->listen(QHostAddress::Any,port)){
        qDebug()<< "listen failed: "<<server->errorString();
        return 0;
    }
    else{
        qDebug()<< "listen on port" <<server->serverPort();
        return 1;
    }
}

QString TcpServer::errorString() const
{
    return server->errorString();
}

void TcpServer::onNewConnection()
{
    clientSocket = server->nextPendingConnection();
    if(clientSocket==nullptr){
        return;
    }
    m_rxBuf.clear();
    connect(clientSocket,&QTcpSocket::readyRead,this,&TcpServer::onReadyRead);
    connect(clientSocket,&QTcpSocket::disconnected,this,&TcpServer::onDisconnected);
    emit clientConnected(clientSocket->peerAddress().toString());
    qDebug()<<"New connection: "<<clientSocket->peerAddress().toString();
}

void TcpServer::onReadyRead()
{
    if(clientSocket==nullptr){
        return;
    }
    m_rxBuf += clientSocket->readAll();
    for(;;){
        int idx = m_rxBuf.indexOf('\n'); //找分隔符
        if(idx<0) break;
        QByteArray line = m_rxBuf.left(idx).trimmed(); //trimmed去掉/r
        m_rxBuf.remove(0,idx+1);    //取出之后删除
        if(!line.isEmpty()){
            emit lineReceived(line);
        }
    }
    if(m_rxBuf.size()>4096){
        m_rxBuf.clear();    //防止爆内存
    }
}

void TcpServer::onDisconnected()
{
    emit clientDisconnected();
    qDebug()<<"Disconnection: "<<clientSocket->peerAddress().toString();
    clientSocket->deleteLater();
    clientSocket=nullptr;
}

void TcpServer::disconnectClient()
{
    if (clientSocket != nullptr)
        clientSocket->disconnectFromHost();
}

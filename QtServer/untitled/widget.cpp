#include "widget.h"
#include "./ui_widget.h"
#include "jsonparse.h"
#include <QDebug>
#include <QString>
#include <QDateTime>
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    m_tcp=new TcpServer(this);
    connect(m_tcp,&TcpServer::lineReceived,this,&Widget::onLine);
    connect(m_tcp, &TcpServer::clientConnected,    this, &Widget::onClientConnected);
    connect(m_tcp, &TcpServer::clientDisconnected, this, &Widget::onClientDisconnected);
    m_tcp->listen(8888);

    m_timer = new QTimer();
    m_timer->start(1000);
    connect(m_timer, &QTimer::timeout, this, &Widget::onHeartbeatTick);
    auto *layout = new QVBoxLayout(ui->chartBox);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_chart = new ChartWidget(ui->chartBox);
    layout->addWidget(m_chart);
    if(!m_store.open("logs/"+QDateTime::currentDateTime()
                                   .toString("yyyy-MM-dd_HH-mm-ss") + ".csv")){
        qDebug()<<"m_store open failed";
    }

    ui->lblConnST->setStyleSheet("color: red;");
}

Widget::~Widget()
{
    delete ui;
}

void Widget::onLine(const QByteArray &line)
{
    Sample s;
    if(!parseSample(line,s)){
        m_bad++;
        ui->lblBad->setText(QString::number(m_bad));
        return;
    }
    s.pcMs=QDateTime::currentMSecsSinceEpoch();
    if(m_haveLast){
        if(s.seq>m_lastSeq+1){
            m_lost+=s.seq-m_lastSeq-1;
        }
        else if(s.seq<=m_lastSeq){
            m_lost=0;
            m_rx=0;
        }//设备重启
    }
    m_lastSeq=s.seq;
    m_lastRxMs=s.pcMs;
    m_haveLast=true;
    m_rx++;
    ui->lblTemp->setText(QString::number(s.t, 'f', 2) + " C");
    ui->lblRx->setText(QString::number(m_rx));
    ui->lblLost->setText(QString::number(m_lost));
    ui->lblHumi->setText(QString::number(s.h,'f',2)+" %");
    m_chart->addSample(s);
    m_store.append(s);
    return;
}

void Widget::onClientConnected(const QString &peer)
{
    ui->lblConnST->setText("已连接");
    ui->lblConnST->setStyleSheet("color: green;");
    m_lastRxMs=0;
}

void Widget::onClientDisconnected()
{
    ui->lblConnST->setText("未连接");
    ui->lblConnST->setStyleSheet("color: red;");
}

void Widget::onHeartbeatTick()
{
    if (m_lastRxMs == 0) {  return; }
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_lastRxMs;

    const double sec = elapsed / 1000.0;

    if (sec <= 3) {
        ui->lblHbST->setText(QStringLiteral("正常"));
        ui->lblHbST->setStyleSheet("color: green;");
    } else {
        ui->lblHbST->setText(QStringLiteral("超时"));
        ui->lblHbST->setStyleSheet("color: red;");
    }
    if(sec>=30){
        m_tcp->disconnectClient();
    }
    return;
}



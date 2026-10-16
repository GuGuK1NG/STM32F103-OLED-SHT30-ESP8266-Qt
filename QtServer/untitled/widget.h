#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include "datastore.h"
#include "tcpserver.h"
#include "chartwidget.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;
private slots:
    void onLine(const QByteArray &line);
    void onClientConnected(const QString& peer);
    void onClientDisconnected();
private:
    TcpServer *m_tcp;
    Ui::Widget *ui;
    quint64 m_rx=0;  //收到的包数
    quint64 m_lost=0; //丢包数
    quint64 m_bad=0; //坏帧数（JSON 解析失败的行数）
    quint32 m_lastSeq=0; //上一个包的包号
    bool m_haveLast=false;    //bool，是否已经收到过第一个包

    ChartWidget *m_chart;

    DataStore m_store;
};
#endif // WIDGET_H

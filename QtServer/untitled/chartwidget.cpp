#include "chartwidget.h"

static void trimSeries(QLineSeries *s)
{
    if (s->count() > MAX_POINTS)
        s->removePoints(0, s->count() - MAX_POINTS);
}

ChartWidget::ChartWidget(QWidget *parent):QChartView(parent) {
    m_chart=new QChart;
    m_serTemp = new QLineSeries;
    m_serHumi = new QLineSeries;
    m_axisX = new QDateTimeAxis;
    m_axisTemp = new QValueAxis;
    m_axisHumi = new QValueAxis;

    m_chart->addAxis(m_axisX,Qt::AlignBottom);
    m_chart->addAxis(m_axisTemp,Qt::AlignLeft);
    m_chart->addAxis(m_axisHumi,Qt::AlignRight);

    m_chart->addSeries(m_serTemp);
    m_chart->addSeries(m_serHumi);

    m_serTemp->attachAxis(m_axisTemp);
    m_serHumi->attachAxis(m_axisHumi);

    m_serTemp->attachAxis(m_axisX);
    m_serHumi->attachAxis(m_axisX);

    m_serTemp->setName(QStringLiteral("温度"));
    m_serHumi->setName(QStringLiteral("湿度"));

    // X 轴：时间
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTitleText("时间");
    m_axisX->setTickCount(6);

    // 左轴：温度
    m_axisTemp->setTitleText("温度 (°C)");
    m_axisTemp->setRange(0, 50);

    // 右轴：湿度
    m_axisHumi->setTitleText("湿度 (%)");
    m_axisHumi->setRange(0, 100);

    setChart(m_chart);

    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    setRenderHint(QPainter::Antialiasing);
}

void ChartWidget::addSample(const Sample &s)
{
    m_serTemp->append(s.pcMs,s.t);
    m_serHumi->append(s.pcMs,s.h);

    const qint64 windowMs = 60 * 5000;
    trimSeries(m_serTemp);
    trimSeries(m_serHumi);
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(s.pcMs - windowMs),
                      QDateTime::fromMSecsSinceEpoch(s.pcMs));
}


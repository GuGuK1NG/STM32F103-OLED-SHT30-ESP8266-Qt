#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QtCharts/QtCharts>
#include "jsonparse.h"

#define MAX_POINTS 400
class ChartWidget : public QChartView
{
    Q_OBJECT
public:
    explicit ChartWidget(QWidget *parent = nullptr);
    void addSample(const Sample& s);
private:
    QChart *m_chart;    //图标本体
    QLineSeries *m_serTemp; //温度曲线
    QLineSeries *m_serHumi; //湿度曲线
    QDateTimeAxis *m_axisX; //X轴（时间）
    QValueAxis *m_axisTemp;  //Y轴（温度）
    QValueAxis *m_axisHumi;  //Y轴（湿度）

};

#endif // CHARTWIDGET_H

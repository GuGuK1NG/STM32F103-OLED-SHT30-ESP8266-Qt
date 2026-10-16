#ifndef JSONPARSE_H
#define JSONPARSE_H

#include <QByteArray>

struct Sample{
    quint32 seq = 0; //包号
    double t=0;
    double h=0;
    qint64 pcMs=0; //收到的时间
};

bool parseSample(const QByteArray &line,Sample &out);


#endif // JSONPARSE_H

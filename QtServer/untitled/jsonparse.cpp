#include "jsonparse.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>


bool parseSample(const QByteArray &line, Sample &out)
{
    QJsonParseError err;
    QJsonDocument js=QJsonDocument::fromJson(line,&err);
    if(!(err.error==QJsonParseError::NoError)){
        return false;
    }
    if((!js["t"].isDouble())||(!js["h"].isDouble())||(!js["seq"].isDouble())){
        return false;
    }
    out.t=js["t"].toDouble();
    out.h=js["h"].toDouble();
    out.seq=js["seq"].toInt();

    return true;
}

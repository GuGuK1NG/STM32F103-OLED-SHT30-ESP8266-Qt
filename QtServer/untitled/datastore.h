#ifndef DATASTORE_H
#define DATASTORE_H
#include <QString>
#include <QFile>
#include <QTextStream>
#include "jsonparse.h"
class DataStore
{
public:
    DataStore();
    ~DataStore();
    bool open(const QString& path); //打开文件，写表头；返回成功与否
    void append(const Sample &s);   //追加一行
    void close();   //关闭（内部会 flush）
    bool isOpen() const;    //查询状态

private:
    QFile m_file;
    QTextStream m_stream;
    bool m_open=false;
};

#endif // DATASTORE_H

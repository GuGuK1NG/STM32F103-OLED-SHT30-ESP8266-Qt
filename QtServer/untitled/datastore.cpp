#include "datastore.h"
#include <QDir>
#include <QDateTime>
DataStore::DataStore() {}

bool DataStore::open(const QString &path)
{
    if(m_open){
        close();
    }
    if(!QDir().mkpath("logs")){
        return false;
    }
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)){
        return false;
    }

    m_stream.setDevice(&m_file);
    m_stream<<"timestamp,seq,temperature_c,humidity_pct\n";
    m_stream.flush();

    m_open=true;
    return true;
}

void DataStore::append(const Sample &s)
{
    if(!m_open||!m_file.isOpen()){
        return;
    }
    QString d=QDateTime::fromMSecsSinceEpoch(s.pcMs).toString("yyyy-MM-dd HH:mm:ss.zzz");
    m_stream << d << ','
             <<s.seq<<','
             << QString::number(s.t,'f',2)  << ','
             << QString::number(s.h,'f',2)  << '\n';
    m_stream.flush();
}

void DataStore::close()
{
    if (!m_open)
        return;

    m_stream.flush();     // 先把缓冲区内容写到 QFile
    m_stream.setDevice(nullptr);   // 断开绑定，避免悬空
    m_file.close();

    m_open = false;
}

bool DataStore::isOpen() const
{
    return m_open && m_file.isOpen();
}
DataStore::~DataStore(){
    close();
}

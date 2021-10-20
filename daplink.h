#ifndef DAP_H
#define DAP_H

#include <QWidget>
#include "QTelnet.h"

#define VID  0x0D28
#define PID  0x0204

class QTimer;
class QString;
class DapLink : public QWidget
{
    Q_OBJECT

public:
    DapLink(QWidget *parent = 0);
    ~DapLink();

    struct SectorInfo{
        bool isProtect;
        quint32 num;
        quint32 address;
        quint32 size;

    };

    struct UIDAddress{
        bool isProtect;
        quint32 num;
        quint32 address;
        quint32 size;

    };

    struct ProgramParameter{
        bool isResetRun;           // reset after programming
        bool isVerify;             // verify after programming
        bool isAutoErase;          // Auto erase sectors before programing
        bool isUnlock;             // Auto erase sectors before programing
    };

    struct TargetInfo{
        bool DualBank;
        qint32 ProductID;                 // 0x 413/414/417
        qint32 FlashSize;                 // kb
        qint32 AddressOffset;             // 起始地址偏移
        quint32 IDCODE;                   // SWIDR IDcode
        quint32 bankCount;                // flash bank counts
        quint32 UID[3];                   //  ID
        QString CurrentTarget;            // 当前目标芯片
        QString DeviceFamily;             // 芯片系列
        QString CPU;                      // Cortex_M3/M4/H7
        QList<SectorInfo *> bank0SectorList;   // 扇区列表
        QList<SectorInfo *> bank1SectorList;   // 扇区列表
    };


    /* function for adapter */
    static QStringList getAdapterSerialNumber();
    QString getAdapterFwVersion();
    void setAdapterMaxClock( quint32 feqKHz );
    void setAdapterResetMode( quint32 resetMode);

    /* function for target */
    void connectTarget(quint32 port );
    void disconnectTarget();
    TargetInfo getTargetInfo(quint32 idcode, bool isDualBanks);
    QString getTargetName();
    void getTargetOptionBytes();
    QByteArray getTargetFlash(quint32 addr, quint32 size, quint32 bytewidth);
    void setTargetProgramParameter(bool, bool, bool, bool);
    void setTargetFullChipErase();
    void setTargetUnlock();
    void setTargetLock();
    void getUserOptions();
    void setTargetFlashProtect(QList<bool > &sectorList);
    void programTargetFile(QString file, quint32 offset); // program file

    /* some parameter */
    ProgramParameter programParam;
    void setMaxClk(const qint32 &value);

signals:
    void signals_setLogtxt(QString);
    void signals_connectStateChanged(Qt::CheckState state);
    void signals_connected();

public slots:

private slots:
    void slots_StateChanged(QAbstractSocket::SocketState s);
    void slots_addText(const char*,int);

private:
    QTelnet *telnet;
    void readDeviceInfo( TargetInfo *info );
    QStringList list;
    void delayMSec(int msec);
    QString fwVersion;
    TargetInfo flashInfo;
    QStringList memList;
    QByteArray memImage;
    QString tmp;
    bool isRxBack = false;
    qint32 maxClk = 2000; // default 2Mhz
};

#endif // DAP_H

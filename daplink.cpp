#include "daplink.h"
#include "QTimer"
#include "QDebug"
#include "hidapi.h"
#include <QThread>
#include <QTime>
#include <QCoreApplication>
#include <QSettings>
#include <QApplication>
#include <QTextCodec>
#include <QtEndian>
#include <QDir>
#include <QHostAddress>

DapLink::DapLink(QWidget *parent)
    : QWidget(parent),
      telnet( new QTelnet )
{
    /* 反馈数据 */
    connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*,int)) );

    /* 状态改变 */
    connect( telnet, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(slots_StateChanged(QAbstractSocket::SocketState)), Qt::DirectConnection );
}

DapLink::~DapLink()
{

}

/**
 * @brief: get device serialnumber 搜索设备串号
 */
QStringList DapLink::getAdapterSerialNumber()
{
    hid_init();
    struct hid_device_info *devs, *cur_dev;
    devs = hid_enumerate(VID, PID);
    cur_dev = devs;
    QStringList list;
    while (cur_dev) {
        list << QString::fromWCharArray( cur_dev->serial_number );
        cur_dev = cur_dev->next;
    }
    list.sort();
    hid_free_enumeration(devs);
    return list ;
}
/**
 * @brief Client::slots_StateChanged 连接状态改变
 * @param socketSta
 */
void DapLink::slots_StateChanged(QAbstractSocket::SocketState socketSta)
{
    switch( socketSta )
    {
    case QAbstractSocket::UnconnectedState:

        emit signals_connectStateChanged( Qt::Unchecked );
        break;
    case QAbstractSocket::HostLookupState:
        emit signals_connectStateChanged( Qt::PartiallyChecked );
        break;
    case QAbstractSocket::ConnectingState:
        emit signals_connectStateChanged( Qt::PartiallyChecked );
        break;
    case QAbstractSocket::ConnectedState:
        emit signals_connectStateChanged( Qt::Checked);
        emit signals_connected();
        break;
    case QAbstractSocket::BoundState:
        emit signals_connectStateChanged( Qt::PartiallyChecked );
        break;
    case QAbstractSocket::ListeningState:
        emit signals_connectStateChanged( Qt::PartiallyChecked );
        break;
    case QAbstractSocket::ClosingState:
        emit signals_connectStateChanged( Qt::PartiallyChecked );
        break;
    }
}

/**
 * @brief Client::slots_addText 反馈信息
 */
void DapLink::slots_addText(const char* data ,int count)
{
    emit signals_setLogtxt(QString ::fromLatin1(data, count));
}

/**
 * @brief Client::connectTarget 连接目标芯片
 */
void DapLink::connectTarget( quint32 port )
{
    telnet->connectToHost("localhost", port);
}
/**
 * @brief Client::disconnectTarget
 */
void DapLink::disconnectTarget()
{
    telnet->disconnectFromHost();
}

/**
 * @brief Client::getFlashInfo 获取芯片flash信息
 */
DapLink::TargetInfo DapLink::getTargetInfo(quint32 idcode, bool isDualBanks)
{
    flashInfo.IDCODE = idcode;
// connect to read flash info
    connect( telnet, &QTelnet::newData, [=](const char* data, int count){
        tmp.append( QString::fromLatin1(data, count) );
    });

    flashInfo.DualBank = isDualBanks;
    qint32 currentbank = 0;
dualbank:
    TargetInfo info;

    tmp.clear();
    QString cmd = tr("flash info %1 sectors").arg(currentbank);
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");

    /* wait for read back */
    QRegExp reg;
    isRxBack = false;
    bool isAddrSizeOk = false;
    bool isPidOk = false;
    info.FlashSize = 0;
    while( !isRxBack ){
        delayMSec(100);
//        reg.setMinimal(false);
        /* 匹配PID */
        reg.setPattern("device id = 0x([0-9a-fA-F]*)\\n");
        if( -1 != reg.indexIn( tmp ) && !isPidOk ){
            bool value;
            info.ProductID = reg.cap(1).toInt(&value, 16) & 0xFFF; //
            isPidOk = true;
        }

        /* 匹配偏移地址,和flash容量大小 */
        reg.setPattern( "#\\d.*at (0x[0-9a-fa-F]{8}), size (0x[0-9a-fa-F]{8})" );
        if( (-1 != reg.indexIn(tmp))  && !isAddrSizeOk ){
            bool isOk;
            info.AddressOffset = reg.cap(1).toInt(&isOk, 16);
            info.FlashSize = reg.cap(2).toInt(&isOk, 16);
            isAddrSizeOk = true;
        }

        /* 匹配扇区信息 */
        reg.setPattern( "(\\d+):\\s*(0x[0-9a-fA-F]+)\\s\\(0x([0-9a-f]*)" );
        qint32 pos = 0;
        bool isOk;
        while( (pos = reg.indexIn( tmp, pos)) != -1 ){
            pos += reg.matchedLength();
            SectorInfo *sector = new SectorInfo();
            sector->num     = reg.cap(1).toInt();
            sector->address = reg.cap(2).toInt(&isOk, 16);
            sector->size    = reg.cap(3).toInt(&isOk, 16);
            info.bank0SectorList<<sector;
        }

        /* 匹配扇区保护信息 */
        reg.setPattern("#.+(\\d+):.*\\n");
        reg.setMinimal(true);
        pos = 0;
        while( (pos = reg.indexIn( tmp, pos)) != -1 ){
            pos += reg.matchedLength();
            QString tmp = reg.cap(0);
            qint32 sec = reg.cap(1).toInt();
            if( tmp.contains( "not protected" ) ){
                info.bank0SectorList.at( sec )->isProtect = false;
            } else {
                info.bank0SectorList.at( sec )->isProtect = true;
            }
        }

        /* 判断读info结束 */
        qint32 size = 0;
        foreach(auto sector, info.bank0SectorList ){
            size += sector->size;
        }
        if( info.FlashSize != size ){
            qDeleteAll( info.bank0SectorList.begin(), info.bank0SectorList.end() ) ;
            info.bank0SectorList.clear(  );
        }else{
            if( (isDualBanks == true) && (currentbank < 1) ){
                currentbank++;
                flashInfo.ProductID = info.ProductID;
                flashInfo.AddressOffset = info.AddressOffset;
                flashInfo.FlashSize = info .FlashSize;
                flashInfo.bank0SectorList = info.bank0SectorList;
                goto dualbank;
            }
            if( isDualBanks) {
                flashInfo.FlashSize += info.FlashSize;
                flashInfo.bank1SectorList = info.bank0SectorList;
            }else {
                flashInfo.ProductID = info.ProductID;
                flashInfo.AddressOffset = info.AddressOffset;
                flashInfo.FlashSize = info .FlashSize;
                flashInfo.bank0SectorList = info.bank0SectorList;
            }
            isRxBack = true;
        }
    }
//    emit signals_setLogtxt(tmp);
    disconnect( telnet, &QTelnet::newData, 0, 0);
    connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*,int)) );

    readDeviceInfo( &flashInfo );


    /* stm32h7x 只能通过选项字节来读取扇区写保护情况 */
    if( flashInfo.CurrentTarget == "stm32h7x" ){
        connect( telnet, &QTelnet::newData, [=](const char* data, int count){
            tmp.append( QString::fromLatin1(data, count) );
        });
        tmp.clear();
        quint16 wrp = 0;
        for(quint32 i=0; i<flashInfo.bankCount; i++){
            cmd = tr("%1 option_read %2 0x38").arg( flashInfo.CurrentTarget ).arg(i);
            telnet->sendData( cmd.toLatin1() );
            telnet->sendData("\n");
            bool isOk = false;
            qint32 timeout = 0;
            while( !isOk && (timeout<30)){
                delayMSec(100);
                reg.setPattern( "<0x\\d+> = (0x[0-9a-fA-F]+)\n");
                if( reg.indexIn( tmp ) != -1){
                    wrp <<= 8;
                    wrp |= (reg.cap(1).toInt(&isOk, 16));
                    tmp.clear();
                    isOk = true;
                }
                timeout++;
            }
        }
        quint8 tmp = wrp>>8;
        for(quint32 i=0; i<8; i++ ){
            flashInfo.bank0SectorList.at(i)->isProtect = !(tmp&0x01);
            tmp>>=1;
        }
        tmp = wrp&0xFF;
        for(quint32 i=0; i<8; i++ ){
            flashInfo.bank1SectorList.at(i)->isProtect = !(tmp&0x01);
            tmp>>=1;
        }
        disconnect( telnet, &QTelnet::newData, 0, 0);
        connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*,int)) );
    }


    return flashInfo;
}

/**
 * @brief DapLink::readDeviceInfo 读取器件信息
 * @param pid
 */
void  DapLink::readDeviceInfo( TargetInfo *info )
{
    QSettings ini( QApplication::applicationDirPath() + "./deviceinfo.ini", QSettings::IniFormat );
    ini.setIniCodec(QTextCodec::codecForName("UTF-8")); // 文件编码需要与文件实际编码一直
    QStringList groups = ini.childGroups();

    /* 根据PID读取芯片型号 */
    foreach (auto group, groups) {
        ini.beginGroup( group );
        bool isOk;
        quint32 pid = ini.value("ProductID").toString().toInt( &isOk, 16);
        if(pid == info->ProductID){
            info->DeviceFamily = ini.value("Device").toString();
            break;
        }else{
            info->DeviceFamily = "unknow device";
        }
        ini.endGroup();
    }
    if( info->DeviceFamily == "unknow device" ){
        info->ProductID = 0;
    }
    /* 根据IDCODE读取CPU */
    QSettings ini_( QApplication::applicationDirPath() + "./device.ini", QSettings::IniFormat );
    ini_.setIniCodec(QTextCodec::codecForName("UTF-8")); // 文件编码需要与文件实际编码一直
    groups.clear();
    groups = ini_.childGroups();

    foreach (auto group, groups) {
        ini_.beginGroup( group );
        bool isOk;
        quint32 idcode = ini_.value("SWDIDCODE").toString().toInt( &isOk, 16);
        if(idcode == info->IDCODE){
            info->CPU = ini_.value("CPU").toString();
            info->bankCount = ini_.value("banks").toInt(&isOk);
            break;
        }
        ini_.endGroup();
    }
}
/**
 * @brief DapLink::readOptionBytes
 */
void DapLink::getTargetOptionBytes()
{

}
/**
 * @brief DapLink::getFwVersion 获取daplink的固件版本
 * @return
 */
QString DapLink::getAdapterFwVersion()
{
    // connect to read dap fw version
    connect( telnet, &QTelnet::newData, [=]( const char* data, int count ){
        QString msg = QString::fromLatin1(data, count);
        /* 匹配固件版本 */
        QRegExp reg( "FW Version = (\\d+\\.\\d+\\.\\d+)" );
        if( -1 != reg.indexIn( msg ) ){
            fwVersion = reg.cap(1);
            isRxBack = true;
        }
    });

    /* send cmd to openocd */
    isRxBack = false;
    QString cmd = "cmsis-dap info";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    /* wait for read back */
    quint32 i = 0;
    while( (!isRxBack) && ( i++<30 )){  // 3000 ms
        delayMSec(100);
    }
    disconnect( telnet, &QTelnet::newData, 0, 0);
    connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*,int)) );
    return fwVersion;
}
/**
 * @brief DapLink::getCurrentTarget 读取当前目标芯片型号,用于后续的指令操作
 * @return
 */
QString DapLink::getTargetName()
{
    // connect to read dap fw version
    connect( telnet, &QTelnet::newData, [=]( const char* data, int count ){
        QString msg = QString::fromLatin1(data, count);
        /* 匹配目标芯片名字 */
        QRegExp reg( "(\\w*).cpu" );
        if( -1 != reg.indexIn( msg ) ){
            flashInfo.CurrentTarget = reg.cap(1);
            isRxBack = true;
        }
    });

    isRxBack = false;
    QString cmd = "target current";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");

    /* wait for read back */
    quint32 i = 0;
    while( (!isRxBack) && ( i++<30 )){  // 3000 ms
        delayMSec(100);
    }
    disconnect( telnet, &QTelnet::newData, 0, 0);
    connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*, int)) );
    return flashInfo.CurrentTarget;
}

/**
 * @brief Programmer::delayMSec 延时,但响应事件循环
 * @param msec
 */
void DapLink::delayMSec(int msec)
{
  QTime dieTime = QTime::currentTime().addMSecs(msec);
  while( QTime::currentTime() < dieTime )
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

void DapLink::setMaxClk(const qint32 &value)
{
    maxClk = value;
}
/**
 * @brief DapLink::getMemoryWord 读取flash数据 整字,32bit
 * @param addr
 * @param size
 * @return
 */
QByteArray DapLink::getTargetFlash(quint32 addr, quint32 count, quint32 bytewidth)
{
    // connect to read memory read world
    disconnect( telnet, &QTelnet::newData, 0, 0);
    tmp.clear();
    connect( telnet, &QTelnet::newData, [=]( const char* data, int cnt ){
        tmp.append( QString::fromLatin1(data, cnt) );
    });

    /* 读取字节 */
    QString cmd = "flash %WIDTH% 0x%1 0x%2";
    switch(bytewidth){
    case 1:
        cmd.replace( "%WIDTH%", "mdb");
        break;
    case 2:
        cmd.replace( "%WIDTH%", "mdh");
        break;
    case 4:
        cmd.replace( "%WIDTH%", "mdw");
        break;
    }
    telnet->sendData( cmd.arg(addr, 0, 16).arg(count / bytewidth, 0, 16).toLatin1() );
    telnet->sendData("\n");

    quint32 timeout = (maxClk + 1) * 3;
WaitForRead:
    delayMSec( count );
    memImage.clear();
    memList.clear();
    QString regstr = ":*\\b([0-9a-f]{%1})\\b\\t*";
    QRegExp reg(regstr.arg(bytewidth*2));
    int pos = 0;
    while ((pos = reg.indexIn(tmp, pos)) != -1) {
        memList << reg.cap(1);
        pos += reg.matchedLength();
    }

    QByteArray array;
    foreach(QString mem, memList){
        array = QByteArray::fromHex( mem.toLocal8Bit() );
        std::reverse(array.begin(),array.end());
        memImage.append( array );
    }
    timeout--;
    if( (memImage.count() != count) &&  timeout ){
        goto WaitForRead;
    }

    tmp.clear();
    memList.clear();
    disconnect( telnet, &QTelnet::newData, 0, 0);
    connect( telnet, SIGNAL(newData(const char*,int)), this, SLOT(slots_addText(const char*,int)) );
    emit signals_setLogtxt(tr("> read 0x%2 bytes from 0x%1\n").arg(addr, 0, 16).arg(count, 0, 16));
    if( timeout == 0 ){
        emit signals_setLogtxt(tr(">> read option timeout,maybe read out protection\n"));
    }
    return memImage;
}
/**
 * @brief DapLink::setSWJMaxClock 设置通信最大频率
 * @param feqKHz
 */
void DapLink::setAdapterMaxClock( quint32 freqKHz )
{
    QString::number(freqKHz);
    telnet->sendData( tr("adapter speed %1").arg(freqKHz).toLatin1() );
    telnet->sendData("\n");
    maxClk = freqKHz;
}

/**
 * @brief DapLink::setSWJResetMode set  reset
 */
void DapLink::setAdapterResetMode( quint32 resetMode)
{
    QString rst;
    if( resetMode == 0 ){
        rst = "sysresetreq";
    }else{
        rst = "vectreset";
    }
    telnet->sendData( tr("cortex_m reset_config %1").arg(rst).toLatin1() );
    telnet->sendData("\n");
}
/**
 * @brief DapLink::setProgramParameter
 * @param reset
 * @param verify
 * @param erase
 */
void DapLink::setTargetProgramParameter( bool reset,
                                         bool verify,
                                         bool erase,
                                         bool unlock)
{
    programParam.isResetRun  = reset;
    programParam.isVerify    = verify;
    programParam.isAutoErase = erase;
    programParam.isUnlock    = unlock;
}

/**
 * @brief DapLink::downloadTargetImage 下载bin文件
 * @param file
 */
void DapLink::programTargetFile(QString file, quint32 offset)
{
    /* halt */
    QString cmd = "halt";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);

    cmd = "report_flash_progress on";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);

    /* 烧录bin文件 */
    bool isBin = false;
    cmd = "flash write_image unlock erase  file ";
    if( file.endsWith(".bin", Qt::CaseInsensitive) ){
        cmd.append( "offset" );
        isBin = true;
    }

    /* 根据选项剔除不需要的操作 */
    if( !programParam.isAutoErase ){
        cmd.remove( "erase" );
    }
    if( !programParam.isUnlock ){
        cmd.remove( "unlock" );
    }

    /* 填充文件名和地址偏移 */
    QString offstr = QString::number(offset, 16);
    cmd.replace("file", file);
    cmd.replace("offset", "0x" + offstr );

    qDebug()<<cmd.toLatin1();
    /* 发送指令 */
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");

    /* 发送完之后校验镜像文件 */
    if( programParam.isVerify ){
        if( isBin ){
            cmd = tr("flash verify_image %1 0x%2 bin").arg( file ).arg( offstr );
        } else {
            cmd = tr("flash verify_image %1").arg( file );
        }
        telnet->sendData( cmd.toLatin1() );
        telnet->sendData("\n");
    }

    /* 复位运行 */
    if( programParam.isResetRun ){
        cmd = tr("reset");
        telnet->sendData( cmd.toLatin1() );
        telnet->sendData("\n");
    }
}
/**
 * @brief DapLink::setTargetFullErase 整片擦除
 */
void DapLink::setTargetFullChipErase()
{
    /* halt */
    QString cmd = "halt";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);

    /* 整片擦除 */
    emit signals_setLogtxt( tr(">> target chip has %1 banks \n").arg(flashInfo.bankCount) );
    for(quint32 i=0; i<flashInfo.bankCount; i++ ){
        cmd = flashInfo.CurrentTarget + tr(" mass_erase %1 ").arg( i );
        telnet->sendData( cmd.toLatin1() );
        telnet->sendData("\n");
    }
    cmd = "reset";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
}
/**
 * @brief DapLink::setTargetUnlock 解除读保护
 */
void DapLink::setTargetUnlock()
{
    /* halt */
    QString cmd = "halt";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);
    emit signals_setLogtxt( tr(">> target chip has %1 banks \n").arg(flashInfo.bankCount) );
    for(quint32 i=0; i<flashInfo.bankCount; i++ ){
        cmd = flashInfo.CurrentTarget + tr(" unlock %1 ").arg( i );
        telnet->sendData( cmd.toLatin1() );
        telnet->sendData("\n");
    }

    cmd = "reset";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
}
/**
 * @brief DapLink::setTargetLock 设定读保护
 */
void DapLink::setTargetLock()
{
    /* halt */
    QString cmd = "halt";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);
    emit signals_setLogtxt( tr(">> target chip has %1 banks \n").arg(flashInfo.bankCount) );
    for(quint32 i=0; i<flashInfo.bankCount; i++ ){
        cmd = flashInfo.CurrentTarget + tr(" lock %1 ").arg( i );
        telnet->sendData( cmd.toLatin1() );
        telnet->sendData("\n");
    }

    cmd = "reset";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
}

/**
 * @brief DapLink::readUserOptions 读取选项字节中的BOR和用户选项
 */
void DapLink::getUserOptions()
{


}
/**
 * @brief DapLink::setTargetFlashProtect
 */
void DapLink::setTargetFlashProtect(QList<bool > &sectorList)
{
    QString cmd = "halt";
    telnet->sendData( cmd.toLatin1() );
    telnet->sendData("\n");
    delayMSec(100);
    qint32 i=0;
    qint32 bankNum = 0;
    qint32 count = flashInfo.bank0SectorList.count();
    foreach ( auto isProtect, sectorList) {
        if( i >= count ){
            i -= count;
            bankNum ++;
        }
        if( isProtect == false){
            QString cmd = tr("flash protect %2 %1 %1 on").arg(i).arg(bankNum);
            telnet->sendData( cmd.toLatin1() );
            telnet->sendData("\n");
        }else{
            QString cmd = tr("flash protect %2 %1 %1 off").arg(i).arg(bankNum);
            telnet->sendData( cmd.toLatin1() );
            telnet->sendData("\n");
        }
        i++;
    }
}

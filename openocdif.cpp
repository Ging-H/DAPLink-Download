#include "openocdif.h"
#include <QSettings>
#include <QProcess>
#include <QDebug>
#include <QDir>
#include <QApplication>
#include <QThread>
#include <QTime>
#include <QRegExp>
#include <QTextCodec>
#include <QMetaEnum>

#define SCRIPTS   "/openocd/share/openocd/scripts"
#define DIR_TARGET  SCRIPTS + "/target"
#define DIR_ADAPTER SCRIPTS + "/interface/cmsis-dap.cfg"

/* MSVC编译器编译中文会出现乱码 */
#if _MSC_VER >=1600 //VS2010版本号是1600
#pragma execution_character_set("utf-8")
#endif


OpenocdIf::OpenocdIf(QObject *parent) : QObject(parent)
{
}

/**
 * @brief OpenocdIf::~OpenocdIf
 */
OpenocdIf::~OpenocdIf()
{
    /* 结束进程 */
    if( !openOcd ){
        return;
    }
    if( openOcd->isOpen() ){
        openOcd->close();
    }
}
/**
 * @brief OpenocdIf::readIDCODE connect and read idcode
 * @return
 */
qint32 OpenocdIf::readIDCODE(QString sn, quint32 port)
{
    isGetIDCODE = false;
    openOcd = new QProcess();

    /* 提取IDCODE */
    connect(openOcd, &QProcess::readyReadStandardError, [=](){
        QString readout = openOcd->readAllStandardError().data();
            QRegExp reg(".*SWD DPIDR 0x([0-9a-fA-F]*)");
            if( -1 != reg.indexIn( readout ) ){
               bool value;
               IDCODE = reg.cap(1).toInt(&value, 16); //
               isGetIDCODE = true;
               disconnect( openOcd, &QProcess::readyReadStandardError, 0, 0);
            }
    });

    /* arguments */
    QStringList arguments;
    arguments << "-f" << QApplication::applicationDirPath() + DIR_ADAPTER;
    arguments << "-c" << ("telnet_port " + QString::number( port ));// 选择端口
    arguments << "-c" << ("cmsis_dap_serial " + sn);// 选择调试器
    arguments << "-c" << "transport select swd";    // SWD跟JTAG共用的两根线,
    arguments << "-f" << QApplication::applicationDirPath() + DIR_TARGET + "/stm32f1x.cfg";// 经测试,f1,f4,h7都可以正常连接,读取IDCODE
    arguments << "-c" << "adapter speed 1000";
    arguments << "-c" << "init;exit";         // 执行完就退出

    /* exe */
    QDir appDir = QApplication::applicationDirPath();
    appDir.cd("openocd");
    appDir.cd("bin");
    QString path = QDir::toNativeSeparators(appDir.path());

    openOcd->start( path += "\\openocd.exe", arguments );
    openOcd->waitForFinished(3000);

    delete openOcd;
    if( isGetIDCODE == false ) // read in slots_readIDCODE()
        return -1;
    else return IDCODE;
}


/**
 * @brief OpenocdIf::getConfigFile get target config.cfg
 * @param IDCODE
 * @return
 */
QString OpenocdIf::getConfigFile(quint32 IDCODE)
{
    QSettings init( QApplication::applicationDirPath() + "./device.ini", QSettings::IniFormat );
    init.setIniCodec(QTextCodec::codecForName("UTF-8")); // 文件编码器需要与文件实际编码一直
    QStringList groups = init.childGroups();
    bool isMatch = false;
    QString config;
    foreach(auto group, groups){
        init.beginGroup(group);
        bool isOk;
        quint32 id = init.value("SWDIDCODE").toString().toInt(&isOk, 16);
        if(id == IDCODE){
            isMatch = true;
            config = init.value("Config").toString();
            if( init.value("banks").toInt() > 1){
                isDualBanks = true;
            }else{
                isDualBanks = false;
            }
            break;
        }else {
            isMatch = false;
        }
        init.endGroup();
    }
    if( !isMatch ){
        return QString();
    }
    return config;
}
/**
 * @brief OpenocdIf::connectMsgOutput 连接输出信号
 */
void OpenocdIf::connectMsgOutput()
{
    connect(openOcd, &QProcess::readyReadStandardOutput,
            this, &OpenocdIf::slots_readOutput, Qt::DirectConnection );
    connect(openOcd, &QProcess::readyReadStandardError,
            this, &OpenocdIf::slots_readError, Qt::DirectConnection );
}
/**
 * @brief OpenocdIf::connectMsgOutput 断开输出信号
 */
void OpenocdIf::disconnectMsgOutput()
{
    disconnect(openOcd, &QProcess::readyReadStandardOutput,
            this, &OpenocdIf::slots_readOutput);
    disconnect(openOcd, &QProcess::readyReadStandardError,
            this, &OpenocdIf::slots_readError);
}
/**
 * @brief OpenocdIf::connectDevice
 * @param adapter  cmsis-dap serial number
 * @param device
 */
void OpenocdIf::connectDevice( QString adapter, QString device, AdapterConfig &config )
{
    openOcd = new QProcess();
    connectMsgOutput();

    /* 设定通信daplink参数 */
    QString port;
    if( config.debugPort == SWD ){
        port = "swd";
    } else {
        port = "jtag";
    }
    QString spd;
    QMetaEnum metaEnum = QMetaEnum::fromType< OpenocdIf::Frequency >();
    spd = metaEnum.valueToKey(config.freq);
    spd = spd.remove("Freq");
    spd = spd.remove("kHz");

    QString rst;
    if( config.resetMode == SysresetReq ){
        rst = "sysresetreq";
    }else{
        rst = "vectreset";
    }
    QStringList arguments;
    arguments << "-f" << adapter; // 加载调试器配置文件
    arguments << "-c" << ("telnet_port " + QString::number( config.port ));// 选择端口
    arguments << "-c" << ("cmsis_dap_serial " + config.serialNum);// 选择调试器
    arguments << "-c" << tr("transport select %1").arg(port);     // 调试接口
    arguments << "-f" << device;  // 加载目标配置文件
    arguments << "-c" << tr("adapter speed %1").arg(spd);
    arguments << "-c" << tr("cortex_m reset_config %1").arg(rst); // 复位模式
    arguments << "-c" << tr("reset_config srst_only");// 仅srst引脚控制复位,没有trst
    arguments << "-c" << tr("init");

    /* 设定openocd.exe的路径 */
    QDir appDir = QApplication::applicationDirPath();
    appDir.cd("openocd");
    appDir.cd("bin");
    QString path = QDir::toNativeSeparators( appDir.path() );

    /* 打开openocd.exe */
    openOcd->start( path += "\\openocd.exe", arguments );
    openOcd->waitForStarted(3000);
}

bool OpenocdIf::getIsDualBanks() const
{
    return isDualBanks;
}

quint32 OpenocdIf::getIDCODE() const
{
    return IDCODE;
}

/**
 * @brief OpenocdIf::getIsConnected
 * @return
 */
bool OpenocdIf::isOpen() const
{
    return openOcd->isOpen();
}

/**
 * @brief OpenocdIf::slots_readOutput 读取输出
 */
void OpenocdIf::slots_readOutput()
{
    QString msgStdOut = openOcd->readAllStandardOutput().data();
    emit signals_standardOutput( msgStdOut );
}
/**
 * @brief OpenocdIf::slots_readError 读取错误信息
 */
void OpenocdIf::slots_readError()
{
    QString msgerr = openOcd->readAllStandardError().data();
    emit signals_standardError(msgerr);
}
/**
 * @brief OpenocdIf::closeExOpenocd 关闭多余的Openocd,保证重新打开之后可以顺利运行
 */
void OpenocdIf::closeExOpenocd()
{
    QProcess task;
    QStringList arg;
    arg<<"-fi"<<"imagename eq Daplink.exe";
    task.start( "tasklist", arg );
    task.waitForStarted(3000);
    task.waitForReadyRead(3000);
    task.waitForFinished(3000);
    QString tasklist = task.readAll();
    task.close();

    QRegExp reg( "(Daplink.exe)");
    int pos = 0;
    arg.clear();
    while ((pos = reg.indexIn(tasklist, pos)) != -1) {
        arg << reg.cap(1);
        pos += reg.matchedLength();
    }
    quint32 proCount = arg.count();

    arg.clear();
    arg<<"-fi"<<"imagename eq openocd.exe";
    task.start( "tasklist", arg );
    task.waitForStarted(3000);
    task.waitForReadyRead(3000);
    task.waitForFinished(3000);
    tasklist = task.readAll();
    task.close();
    reg.setPattern( "(openocd.exe)" );
    pos = 0;
    arg.clear();
    while ((pos = reg.indexIn(tasklist, pos)) != -1) {
        arg << reg.cap(1);
        pos += reg.matchedLength();
    }
    quint32 opdCount = arg.count();
    /* Program.exe = 1, openocd.exe > 1 ,意味着有多余的openocd.exe */
    if( (opdCount>=1) && (proCount==1) ){
        arg.clear();
        arg<<"-f"<<"-im"<<"openocd.exe"<<"-t";
        task.start( "taskkill", arg );
        task.waitForStarted(3000);
        task.waitForReadyRead(3000);
        task.waitForFinished(3000);
        tasklist = task.readAll();
        task.close();
    }
}
/**
 * @brief OpenocdIf::allocOpenocdPort 根据已经打开的应用数量重新分配端口号
 * @return
 */
quint32 OpenocdIf::allocOpenocdPort()
{
    closeExOpenocd();// 关闭多余的openocd,避免打开过多的应用
    qsrand( QTime::currentTime().toString("mmss").toInt() );
    quint32 tmp = 0;
    while( (tmp < 1024) || (tmp > 65535)){
        tmp = qrand();
    }
    emit signals_standardOutput( "server port : " + QString::number( tmp ) + "\r\n");
    return tmp;
}

/**
 * @brief OpenocdIf::slots_connectDevice 连接设备
 */
OpenocdIf::ConnectErr OpenocdIf::execute( AdapterConfig &config  )
{
    config.port = allocOpenocdPort();
//    emit signals_standardError( "telnect port " + QString::number( config.port ));
    /* 读取IDCODE */
    qint32 idcode = readIDCODE(config.serialNum, config.port);
    if( idcode == -1){
        emit signals_standardOutput( tr("无法读取IDCODE,\n 未连接目标\n") );
        return NoTarget;
    }

    /* 打开文件 device.ini,读取 *.cfg */
    QString configFile = getConfigFile( idcode );
    if( configFile.isEmpty() ){
        emit signals_standardOutput( tr("无法打开%1\n 未连接目标\n").arg( configFile ) );
        return NoConfigFile;
    }

    /* connnect device */
    QString path = QApplication::applicationDirPath() + SCRIPTS + "/interface/cmsis-dap.cfg";
    configFile = QApplication::applicationDirPath() + SCRIPTS + "/target" + configFile;
    connectDevice( path, configFile, config);

    /* 打开openocd.exe  */
    if( openOcd->isOpen() ) {
        disconnectMsgOutput();
        return NoError;
    }else{
        emit signals_standardOutput( tr("something worring, not connected\n") );
        return NotConnect;
    }
}

/**
 * @brief OpenocdIf::slots_closeExe
 */
void OpenocdIf::terminate()
{
    /* 结束进程 */
    if( openOcd->isOpen() ){
        openOcd->close();
    }
}

#ifndef OPENOCDIF_H
#define OPENOCDIF_H

#include <QObject>

class QProcess;

class OpenocdIf : public QObject
{
    Q_OBJECT
public:
    explicit OpenocdIf(QObject *parent = nullptr);
    OpenocdIf::~OpenocdIf();


    enum DebugPort{
        SWD,
        JTAG,
    };

    enum Frequency{
        Freq2000kHz,
        Freq1000kHz,
        Freq500kHz,
        Freq200kHz,
        Freq100kHz,
        Freq50kHz,
        Freq20kHz,
        Freq10kHz,
        Freq5kHz,
    };
    Q_ENUM(Frequency)

    enum ResetMode{
        SysresetReq,
        Vectreset,
    };

    enum ConnectErr{
        NoError,
        NoTarget,
        NoConfigFile,
        NotConnect,
    };

    struct AdapterConfig{
        quint32    port;
        QString    serialNum;
        DebugPort  debugPort;
        Frequency  freq;
        ResetMode  resetMode;
    };

    bool isOpen() const;
    ConnectErr execute(AdapterConfig &config);
    void terminate();

    void connectMsgOutput();
    void disconnectMsgOutput();


    quint32 getIDCODE() const;

    quint32 port = 0;

    bool getIsDualBanks() const;

signals:
    void signals_standardError(QString);
    void signals_standardOutput(QString);

public slots:



private slots:
    void slots_readOutput();
    void slots_readError();

private:
    bool isGetIDCODE = false;
    QProcess *openOcd = nullptr;
    quint32 allocOpenocdPort();


    qint32 readIDCODE(QString sn, quint32 port);
    QString getConfigFile(quint32 IDCODE); // get idcode from device*.ini
    void connectDevice( QString adapter, QString device, AdapterConfig &config ); // connect
    quint32 IDCODE = 0;
    bool isDualBanks = false;


    void closeExOpenocd();


};

#endif // OPENOCDIF_H

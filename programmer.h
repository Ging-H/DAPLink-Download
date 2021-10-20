#ifndef DAPPROGRAMMER_H
#define DAPPROGRAMMER_H

#include <QMainWindow>
#include <daplink.h>
#include <openocdif.h>

namespace Ui {
class Programmer;
}
class Memory;
class QThread ;
class Client;

class Programmer : public QMainWindow
{
    Q_OBJECT
    enum DataWidth{
        Bit32,
        Bit16,
        Bit8,
    };


public:
    explicit Programmer(QWidget *parent = 0);
    ~Programmer();

    DataWidth getDataWidth() const;
    void setDataWidth(const DataWidth &value);

    void closeEvent(QCloseEvent *event);

protected:
    bool eventFilter(QObject *watched, QEvent *event);


signals:
    void signals_startOpenocd();
    void signals_closeOpenocd();




private slots:
    void slots_logMsg(QString msg);

    void on_btnDeviceConnect_clicked();

    void on_ckbFullChipErase_clicked(bool checked);

    void slots_readFlashInfo();

    void slots_ledToggle(Qt::CheckState state);

    void on_btnReflash_clicked();

    void on_cbbPort_currentIndexChanged(int index);

    void on_cbbFreq_currentIndexChanged(int index);

    void on_cbbResetMode_currentIndexChanged(int index);

    void on_ckbRunAfterProg_clicked(bool checked);

    void on_ckbVerify_clicked(bool checked);

    void on_btnDownload_clicked();

    void on_btnOpen_clicked();

    void on_cbbSerialNumber_currentTextChanged(const QString &arg1);

    void on_ckbUnlock_clicked(bool checked);

    void on_btnFullErase_clicked();

    void on_btnUnlock_clicked();

    void on_btnLock_clicked();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_txtLog_textChanged();

    void on_btnNetDownload_clicked();

    void on_tabOption_currentChanged(int index);

private:
    Ui::Programmer *ui = nullptr;
    void Programmer::showOnTable(QByteArray data, DataWidth dataWidth);
    DataWidth dataWidth = Bit32;
    Memory *deviceMem = nullptr;
    void reloadFile(Memory *mem, QString path);
    OpenocdIf *openocd = nullptr;
    DapLink *daplink = nullptr;
    bool isConnected = false;
    bool isDownloadStart = false;
    bool isDownload = false;


    void delayMSec(int msec);
    void updateUiInfo( DapLink::TargetInfo info, QString fw  );
    OpenocdIf::AdapterConfig adapter;
    void loadImgFile( QString file );
    void resetConnect();







};

#endif // DAPPROGRAMMER_H

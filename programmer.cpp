#include "programmer.h"
#include "ui_programmer.h"
#include "daplink.h"
#include <QMimeData>
#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>
#include "memory.h"
#include "hextobin.h"
#include "openocdif.h"
#include <QThread>
#include <QTimer>
#include <QProcess>
#include <QTime>
#include <QMetaEnum>
#include <QFileDialog>
#include <QStandardPaths>
#include "windows.h"

/**
  * @UI Thread
  */

/* MSVC编译器编译中文会出现乱码 */
#if _MSC_VER >=1600 //VS2010版本号是1600
#pragma execution_character_set("utf-8")
#endif

/**
 * @brief DAPProgrammer::DAPProgrammer
 * @param parent
 */
Programmer::Programmer(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Programmer)
{
    ui->setupUi(this);
    this->setWindowTitle(tr("DapLink for STM32F1/F4/H7 v1.0"));
    /* 启动搜索串号 */
    QStringList serialList = DapLink::getAdapterSerialNumber();
    ui->cbbSerialNumber->addItems( serialList );
    slots_logMsg(tr("serial number:\n"));
    foreach(auto msg, serialList){
        slots_logMsg(msg +"\n");
    }

    /* 拖拽释放事件 */
    ui->tabOption->installEventFilter( this ); // 将taboption安装到事件过滤器中,作为监控对象,监控所有事件
    qint32 i = ui->tabOption->count();
    for(; i>0; ){
        i--;
        ui->tabOption->tabBar()->setTabButton(i, QTabBar::RightSide, NULL);// 隐藏特定页面的关闭按钮
    }

    /* 初始显示*/
    deviceMem = new Memory( ui->tabOption->widget(0) );
    ui->tabOption->repaint();
    ui->tabOption->setCurrentIndex(0);
    connect( deviceMem, &Memory::signals_readMemory, [=](){
        if( !isConnected ){
            return;
        }
        Memory *mem = qobject_cast<Memory *>(ui->tabOption->widget(0)->children().at(0));
        mem->btnRead->setEnabled(false);
        quint32 addr = mem->spbAddress->value();
        quint32 size = mem->spbSize->value();
        QMetaEnum metaEnum = QMetaEnum::fromType<Memory::DataWidth>();
        quint32 byreWidth = metaEnum.keyToValue( mem->cbbDataWidth->currentText().toLatin1() );
        QByteArray array = daplink->getTargetFlash(addr, size, byreWidth);
        mem->clearItem();
        mem->insertTableItem(addr, array);
        mem->btnRead->setEnabled(true);
    });

    // tab 关闭按钮,销毁对象,释放空间
    connect(ui->tabOption, &QTabWidget::tabCloseRequested, [=](int index){
        QWidget *widget = ui->tabOption->widget(index);
        ui->tabOption->removeTab(index);
        delete widget;
    });
    adapter.debugPort = OpenocdIf::SWD;
    adapter.freq = OpenocdIf::Freq2000kHz;
    adapter.resetMode = OpenocdIf::SysresetReq;
}

/**
 * @brief DAPProgrammer::~DAPProgrammer
 */
Programmer::~Programmer()
{
    // TODO: 记录下载选项并加载
    delete ui;
}

/**
 * @brief Programmer::loadImgFile 加载镜像文件
 * @param file
 */
void Programmer::loadImgFile( QString path )
{
    /* 打开文件 */
    QFileInfo info(path);
    QString suffix = info.suffix();
    if( !suffix.contains("hex", Qt::CaseInsensitive) && !suffix.contains("bin", Qt::CaseInsensitive) ){
        QMessageBox::critical(this, tr("打开文件"), tr("不支持的文件类型,仅支持<*.hex, *.bin>"), QMessageBox::Cancel );
        return ;
    }
    QFile file(path);
    if( !file.open(QIODevice::ReadOnly) ){
        QMessageBox::critical(NULL, tr("文件打开错误"), tr("文件打开错误:%1").arg(file.errorString()), QMessageBox::Cancel);
        return ;
    }

    /* 记录文件路径 */
    ui->txtFilePath->setText( path );

    /* 读取文件数据 */
    QWidget *wid =  new QWidget();
    Memory *mem  =  new Memory(wid);
    connect(mem, &Memory::signals_msg, this, &Programmer::slots_logMsg );
    mem->setFilePath( path );
    QString fileName = info.fileName();
    ui->tabOption->addTab( wid, fileName ); // 新建选项卡
    // 根据文件类型打开不同的文件
    if( suffix.contains("hex", Qt::CaseInsensitive) ){
        /* hex 文件 */
        HexToBin *hexSeg = new HexToBin( &file );
        mem->writeHexFile(hexSeg->segmentList());
        delete hexSeg;
    }else if( suffix.contains("bin", Qt::CaseInsensitive) ){
        /* bin 文件*/
        QByteArray binArray = file.readAll();
        mem->writeBinFile( binArray );
    }

    /* 重新读取文件数据 */
    connect( mem, &Memory::signals_readMemory, [=](){
        Memory *mem = qobject_cast<Memory *>(ui->tabOption->currentWidget()->children().at(0));
        this->reloadFile(mem, mem->getFilePath());
    });

    file.close();// 关闭文件
    ui->tabOption->setCurrentIndex( ui->tabOption->count()-1 );// 聚焦到新选项卡
}

/**
 * @brief Programmer::eventFilter 事件过滤器, 拖动文件到软件上显示
 * @param watched 观察对象
 * @param event   触发事件
 * @return
 */
bool Programmer::eventFilter(QObject *watched, QEvent *event)
{
    if( watched == ui->tabOption ){
        /* 拖进 */
        if( QEvent::DragEnter == event->type() ){
            QDragEnterEvent *dragEvent = dynamic_cast< QDragEnterEvent * >(event);
            dragEvent->acceptProposedAction();
        }else if( QEvent::Drop == event->type() ){
            /* 放下 */
            QDropEvent *dropEvent = dynamic_cast< QDropEvent * >(event);
            QList<QUrl> urls = dropEvent->mimeData()->urls();
            foreach (auto path, urls) {
                QString file = path.toLocalFile();
                loadImgFile(file);
            }
            ui->txtFilePath->setText( urls.at(0).toLocalFile() );
            return true;

        }
    }
    return QMainWindow::eventFilter(watched, event);
}

/**
 * @brief Programmer::getDataWidth 读取数据宽度
 * @return
 */
Programmer::DataWidth Programmer::getDataWidth() const
{
    return dataWidth;
}

/**
 * @brief Programmer::setDataWidth 设置数据宽度
 * @param value
 */
void Programmer::setDataWidth(const DataWidth &value)
{
    dataWidth = value;
}

/**
 * @brief Programmer::loadImgFile
 * @param mem
 * @param path
 */
void Programmer::reloadFile(Memory *mem, QString path)
{
    QFile file(path);
    if( !file.open(QIODevice::ReadOnly) ){
        QMessageBox::critical(NULL, tr("文件打开错误"), tr("文件打开错误"), QMessageBox::Cancel);
        return ;
    }
    QFileInfo info(path);
    QString suffix = info.suffix();
    if( !suffix.contains("hex", Qt::CaseInsensitive) && !suffix.contains("bin", Qt::CaseInsensitive) ){
        QMessageBox::critical(this, tr("打开文件"), tr("不支持的文件类型,仅支持*.hex, *.bin"), QMessageBox::Cancel );
        return ;
    }

    // 根据文件类型打开不同的文件
    if( suffix.contains("hex", Qt::CaseInsensitive) ){
        /* hex 文件 */
        HexToBin *hexSeg = new HexToBin( &file );
        mem->writeHexFile(hexSeg->segmentList());
        delete hexSeg;

    }else if( suffix.contains("bin", Qt::CaseInsensitive) ){
        /* bin 文件*/
        QByteArray binArray = file.readAll();
        mem->writeBinFile( binArray );
    }
}

/**
 * @brief Programmer::on_btnDeviceConnect_clicked 连接目标芯片
 */
void Programmer::on_btnDeviceConnect_clicked()
{
    if( ui->btnDeviceConnect->isChecked() ){
        openocd = new OpenocdIf();
        /* openocd.exe */
        connect(openocd, &OpenocdIf::signals_standardOutput,
                this, &Programmer::slots_logMsg );
        connect(openocd, &OpenocdIf::signals_standardError,
                this, &Programmer::slots_logMsg );

        OpenocdIf::ConnectErr isOk = openocd->execute( adapter ); // 连接目标芯片
        if( isOk == OpenocdIf::NoError ){
            /* telnet 客户端连接 */
            daplink = new DapLink(this);
            connect(daplink, &DapLink::signals_setLogtxt, this,&Programmer::slots_logMsg);
            connect(daplink, &DapLink::signals_connectStateChanged ,this, &Programmer::slots_ledToggle, Qt::DirectConnection );
            connect(daplink, &DapLink::signals_connected, this, &Programmer::slots_readFlashInfo );
            daplink->connectTarget( adapter.port );
        } else {
            delete openocd;
            openocd = nullptr;
            ui->btnDeviceConnect->setChecked(false);
        }
    } else {
        emit daplink->signals_connectStateChanged(Qt::Unchecked);
    }
}
/**
 * @brief Programmer::resetConnect 断开连接
 */
void Programmer::resetConnect()
{
    if( daplink != nullptr ){
        daplink->disconnectTarget();
        delete daplink;
        daplink = nullptr;
    }
    /* 关闭openocd.exe */
    if( openocd != nullptr ){
        openocd->terminate();
        delete openocd;
        openocd = nullptr;
    }
    ui->lblDeviceFamily->setText("--");
    ui->lblProductID->setText("--");
    ui->lblFlashSize->setText("--");
    ui->lblFWVersion->setText("--");
    ui->lblIDCODE->setText("--");
    ui->lblProcessor->setText("--");
}
/**
 * @brief Programmer::slots_logMsg 记录信息
 * @param msg
 */
void Programmer::slots_logMsg(QString msg)
{
    ui->txtLog->moveCursor(QTextCursor::End);
    ui->txtLog->insertPlainText(msg);
    ui->txtLog->moveCursor(QTextCursor::End);
}

/**
 * @brief Programmer::closeEvent 关闭窗口
 * @param event
 */
void Programmer::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)

    if( daplink != nullptr ){
        daplink->disconnectTarget();
        delete daplink;
        daplink = nullptr;
    }
    /* 关闭openocd.exe */
    if( openocd != nullptr ){
        openocd->terminate();
        delete openocd;
        openocd = nullptr;
    }
}

/**
 * @brief Programmer::slots_ledToggle toggleLED
 * @param state
 */
void Programmer::slots_ledToggle(Qt::CheckState state)
{
    switch(state){
    case Qt::Unchecked:
        ui->led->setLedOffColor(Led::Red);
        ui->led->setChecked(false);
        ui->btnDeviceConnect->setText( tr("Disconnected") );
        ui->btnDeviceConnect->setChecked(false);
        slots_logMsg( tr(">> Disconnected\n") );
        isConnected = false;
        ui->cbbPort->setEnabled(true);
        ui->cbbSerialNumber->setEnabled(true);
        ui->btnDeviceConnect->setEnabled(true);
        resetConnect();
        break;
    case Qt::PartiallyChecked:
        ui->btnDeviceConnect->setText( tr("Host Lookup") );
        ui->led->setLedOffColor(Led::Yellow);
        ui->led->setChecked(false);
        slots_logMsg( tr(">> Host Lookup\n") );
        ui->cbbPort->setEnabled(false);
        ui->btnDeviceConnect->setEnabled(false);

        break;
    case Qt::Checked:
        ui->led->setLedOnColor(Led::Green);
        ui->led->setChecked(true);
        ui->btnDeviceConnect->setText( tr("Device Connected") );
        slots_logMsg( tr(">> Device Connected\n") );
        ui->tabOption->setCurrentIndex(0);
        ui->cbbPort->setEnabled(false);
        ui->cbbSerialNumber->setEnabled(false);
        daplink->setTargetProgramParameter( ui->ckbRunAfterProg->isChecked(),
                                            ui->ckbVerify->isChecked(),
                                            ui->ckbFullChipErase->isChecked(),
                                            ui->ckbUnlock->isChecked());

        break;
    }
}

/**
 * @brief Programmer::on_btnReflash_clicked
 */
void Programmer::on_btnReflash_clicked()
{
    if( ui->btnDeviceConnect->isChecked() ){
        return;
    }
    /* 启动搜索串号 */
    QStringList serialList =  DapLink::getAdapterSerialNumber();
    ui->cbbSerialNumber->clear();
    ui->cbbSerialNumber->addItems( serialList );
}

/**
 * @brief Programmer::slots_readFlashInfo 在连接target之后读取芯片信息
 */
void Programmer::slots_readFlashInfo()
{
    daplink->getTargetName();// get target name for further use
    QString fwVersion = daplink->getAdapterFwVersion();
    quint32 idcode = openocd->getIDCODE();
    DapLink::TargetInfo info =  daplink->getTargetInfo( idcode , openocd->getIsDualBanks());
    this->updateUiInfo( info, fwVersion );
    daplink->setMaxClk(adapter.freq);// 用于调整读取flash的超时时间

    /* 读取flash 内存数据 */
    Memory *mem = qobject_cast<Memory *>(ui->tabOption->widget(0)->children().at(0));
    quint32 addr = mem->spbAddress->value();
    quint32 size = mem->spbSize->value();
    QMetaEnum metaEnum = QMetaEnum::fromType<Memory::DataWidth>();
    quint32 byreWidth = metaEnum.keyToValue( mem->cbbDataWidth->currentText().toLatin1() );
    QByteArray array = daplink->getTargetFlash(addr, size, byreWidth);
    mem->clearItem();
    mem->insertTableItem(addr, array);
    ui->btnDeviceConnect->setEnabled(true);


    /* 扇区写保护 */ // TODO: 添加STM31F1的写保护设置功能
    ui->tblWRP->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tblWRP->setRowCount(0);   // clear
    ui->tblWRP->setRowCount(info.bank0SectorList.count() + info.bank1SectorList.count() );

    QTableWidgetItem *item = nullptr;
    quint32 row = 0;
    QCheckBox *ckb;
    foreach (auto sector, info.bank0SectorList) {
        item = new QTableWidgetItem();
        item->setText( tr("bank0 nWRP%1").arg( sector->num ) );
        item ->setTextAlignment(Qt::AlignHCenter);
        ui->tblWRP->setItem( row , 0, item );
        item = nullptr;

        ckb = new QCheckBox;
        QHBoxLayout *layout = new QHBoxLayout();
        layout->setAlignment(Qt::AlignCenter);
        layout->addWidget(ckb);
        layout->setMargin(0);
        ckb->setChecked( !sector->isProtect  );
        QWidget *wid = new QWidget();
        wid->setLayout(layout);
        ui->tblWRP->setCellWidget( row , 1, wid );

        item = new QTableWidgetItem();
        item->setText( "0: 写保护使能\r\n"
                       "1: 写保护未使能" );
        item ->setTextAlignment(Qt::AlignLeft);
        ui->tblWRP->setItem( row , 2, item );
        item = nullptr;

        row++;
    }

    /* stm32h7 */
    if( info.DualBank ){
        foreach (auto sector, info.bank1SectorList) {
            item = new QTableWidgetItem();
            item->setText( tr("bank1 nWRP%1").arg( sector->num ) );
            item ->setTextAlignment(Qt::AlignHCenter);
            ui->tblWRP->setItem( row , 0, item );
            item = nullptr;

            ckb = new QCheckBox;
            QHBoxLayout *layout = new QHBoxLayout();
            layout->setAlignment(Qt::AlignCenter);
            layout->addWidget(ckb);
            layout->setMargin(0);
            ckb->setChecked( !sector->isProtect  );
            QWidget *wid = new QWidget();
            wid->setLayout(layout);
            ui->tblWRP->setCellWidget( row , 1, wid );

            item = new QTableWidgetItem();
            item->setText( "0: 写保护使能\r\n"
                           "1: 写保护未使能" );
            item ->setTextAlignment(Qt::AlignLeft);
            ui->tblWRP->setItem( row , 2, item );
            item = nullptr;

            row++;
        }
    }
    isConnected = true;
}

/**
 * @brief Programmer::updateUiMsg 更新ui的显示
 */
void Programmer::updateUiInfo( DapLink::TargetInfo info, QString fw )
{
    ui->lblFWVersion->setText( fw );
    if( info.ProductID == 0 ){
        ui->lblDeviceFamily->setText( info.CurrentTarget );
    } else {
        ui->lblDeviceFamily->setText( info.DeviceFamily );
    }
    ui->lblProductID->setText( "0x" + QString::number(info.ProductID, 16) );
    ui->lblFlashSize->setText( QString::number(info.FlashSize/1024, 10) + "kbytes" );
    ui->lblIDCODE->setText("0x" +  QString::number(info.IDCODE, 16).toUpper() );
    ui->lblProcessor->setText( info.CPU );
    ui->spbAddress->setValue( info.AddressOffset );
}

/**
 * @brief Programmer::delayMSec 延时,但响应事件循环
 * @param msec
 */
void Programmer::delayMSec(int msec)
{
  QTime dieTime = QTime::currentTime().addMSecs(msec);
  while( QTime::currentTime() < dieTime )
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}

/**
 * @brief Programmer::on_cbbPort_currentIndexChanged 修改通信模式
 * @param index
 */
void Programmer::on_cbbPort_currentIndexChanged(int index)
{
    adapter.debugPort = (OpenocdIf::DebugPort)index;
}
/**
 * @brief Programmer::on_cbbSerialNumber_currentTextChanged 修改调试器串号
 * @param arg1
 */
void Programmer::on_cbbSerialNumber_currentTextChanged(const QString &arg1)
{
    adapter.serialNum = arg1;
}


/**
 * @brief Programmer::on_cbbFreq_currentIndexChanged 修改通信频率
 * @param index
 */
void Programmer::on_cbbFreq_currentIndexChanged(int index)
{
    adapter.freq = (OpenocdIf::Frequency)index;
    QString spd;
    QMetaEnum metaEnum = QMetaEnum::fromType< OpenocdIf::Frequency >();
    spd = metaEnum.valueToKey(adapter.freq);
    spd = spd.remove("Freq");
    spd = spd.remove("kHz");
    if ( isConnected ){
        daplink->setAdapterMaxClock( spd.toInt() );
    }
}

/**
 * @brief Programmer::on_cbbResetMode_currentIndexChanged 修改复位模式
 * @param index
 */
void Programmer::on_cbbResetMode_currentIndexChanged(int index)
{
    adapter.resetMode = (OpenocdIf::ResetMode)index;
    if ( isConnected ){
        daplink->setAdapterResetMode( index );
    }
}

/**
 * @brief Programmer::on_ckbRunAfterProg_clicked // 下载后自动运行
 * @param checked
 */
void Programmer::on_ckbRunAfterProg_clicked(bool checked)
{
    if( isConnected ){
        daplink->programParam.isResetRun = checked;
    }
}

/**
 * @brief Programmer::on_ckbVerify_clicked // 下载后校验
 * @param checked
 */
void Programmer::on_ckbVerify_clicked(bool checked)
{
    if( isConnected ){
        daplink->programParam.isVerify = checked;
    }
}

/**
 * @brief Programmer::on_ckbFullChipErase_clicked // 擦除扇区
 * @param checked
 */
void Programmer::on_ckbFullChipErase_clicked(bool checked)
{
    if( isConnected ){
        daplink->programParam.isAutoErase = checked;
    }
}
/**
 * @brief Programmer::on_ckbUnlock_clicked // 自动解除读保护
 * @param checked
 */
void Programmer::on_ckbUnlock_clicked(bool checked)
{
    if( isConnected ){
        daplink->programParam.isUnlock = checked;
    }

}
/**
 * @brief Programmer::on_btnDownload_clicked 下载文件
 */
void Programmer::on_btnDownload_clicked()
{
    isDownload = true;
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标.\n") );/* 未连接 目标*/
        isDownload = false;
        return;
    }

    QString file = ui->txtFilePath->text();

    QFileInfo info(file);
    if(!info.isFile()){
        slots_logMsg( tr(">> error: 无法打开文件 %1 \n").arg(file) );/* 非文件 */
        isDownload = false;
        return;
    }

    /* 下载 */
    quint32 offset = ui->spbAddress->value();
    if( !info.suffix().compare("hex", Qt::CaseInsensitive) || !(info.suffix().compare("bin", Qt::CaseInsensitive)) ){
        ui->btnDownload->setEnabled(false);
        ui->pgb->setMaximum(0);
        ui->pgb->setValue(0);
        isDownloadStart = false;

        /* 创建临时目录/*.bin,避开中文和空格 */
        QString newName = QTime::currentTime().toString("~hh-mm-ss-zzz") + "." + info.suffix();
        qDebug()<<QDir::tempPath() + "/" + newName;
        QString tempfile = QDir::tempPath() + "/" + newName;
        QFile image(file);
        bool isOk = image.copy(tempfile);
        tempfile = QDir::fromNativeSeparators( tempfile );

        SetFileAttributesW( (LPCWSTR)tempfile.unicode(), FILE_ATTRIBUTE_HIDDEN );
        slots_logMsg( tr("> 创建镜像副本:%1\n").arg(tempfile) );

        if( isOk ){
            daplink->programTargetFile( tempfile, offset );
        }
        ui->btnDownload->setEnabled(true);
    }else{
        slots_logMsg( tr(">> error: 不支持的文件类型, 只支持 *.hex/*.bin 文件的下载 \n") );
        return;
    }
}

/**
 * @brief Programmer::on_btnOpen_clicked 加载文件路径
 */
void Programmer::on_btnOpen_clicked()
{
    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QStringList paths = QFileDialog::getOpenFileNames( nullptr,
                                   "Open File",
                                   desktop,
                                   "images (*.hex *.bin)" );

    if( paths.isEmpty() )
        return;
    foreach(auto path, paths){
        loadImgFile(path);
    }
    ui->txtFilePath->setText( paths.at(0) );
}

/**
 * @brief Programmer::on_btnFullErase_clicked 整片擦除
 */
void Programmer::on_btnFullErase_clicked()
{
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标 \n") );/* 未连接 目标*/
        return;
    }
    qint32 btn = QMessageBox::information(nullptr, "整片擦除",
                                      "部分芯片包含有多个flash banks,\n"
                                      "整片擦除将会擦除所有flash banks的数据,\n"
                                      "\t确定?",
                                      "确定", "取消");
    if( btn == 0){
       daplink->setTargetFullChipErase();
    }
}
/**
 * @brief Programmer::on_btnUnlock_clicked 解除读保护
 */
void Programmer::on_btnUnlock_clicked()
{
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标 \n") );/* 未连接 目标*/
        return;
    }
    qint32 btn = QMessageBox::information(nullptr, "解除读保护",
                                      "部分芯片包含有多个flash banks,\n"
                                      "解除读保护将会擦除所有数据,\n"
                                      "\t确定?",
                                      "确定", "取消");
    if( btn == 0){
       daplink->setTargetUnlock();
    }
}
/**
 * @brief Programmer::on_btnLock_clicked
 */
void Programmer::on_btnLock_clicked()
{
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标 \n") );/* 未连接 目标*/
        return;
    }
    qint32 btn = QMessageBox::information(nullptr, "读保护",
                                      "设置读保护之后将无法读取flash,但可以在解除读保护之后读取,\n"
                                      "读保护功能将在断开调试器并断电之后生效,\n"
                                      "\t确定?",
                                      "确定", "取消");
    if( btn == 0 ){
        daplink->setTargetLock();
    }
}
/**
 * @brief Programmer::on_pushButton_clicked 设定写保护
 */
void Programmer::on_pushButton_clicked()
{
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标 \n") );/* 未连接 目标*/
        return;
    }
    qint32 count = ui->tblWRP->rowCount();
    QList<bool> ptList;
    QCheckBox *ckb;

    for(qint32 i=0; i<count; i++){
        ckb = qobject_cast<QCheckBox *>(ui->tblWRP->cellWidget(i, 1)->children().at(1));
        if( ckb->isChecked() ){
            ptList<<true;
        }else{
            ptList<<false;
        }
    }
    daplink->setTargetFlashProtect( ptList );
}

/**
 * @brief Programmer::on_pushButton_2_clicked 写保护恢复默认//不使能写保护
 */
void Programmer::on_pushButton_2_clicked()
{
    if( !isConnected ){
        slots_logMsg( tr(">> error: 未连接目标 \n") );/* 未连接 目标*/
        return;
    }

    qint32 count = ui->tblWRP->rowCount();
    QList<bool> ptList;
    QCheckBox *ckb;

    for(qint32 i=0; i<count; i++){
        ckb = qobject_cast<QCheckBox *>(ui->tblWRP->cellWidget(i, 1)->children().at(1));
        ckb->setChecked(true);
        ptList<<true;
    }
    daplink->setTargetFlashProtect( ptList );
}
/**
 * @brief Programmer::on_txtLog_textChanged 提取文本更新下载进度条
 */
void Programmer::on_txtLog_textChanged()
{
    qint32 num = 0;
    QString txt = ui->txtLog->toPlainText();
    QRegExp reg;
    QTextStream stm ( &txt );

    if( isDownloadStart == false ){
        /* 开始判定 */
        qint32 index = txt.lastIndexOf( "flash_write_progress_async_start" );
        if( index == -1 ){
            return;
        }
        stm.seek( index );
        txt = stm.readLine();

        reg.setPattern("0x([0-9a-fA-F]+)");
        if( reg.indexIn( txt ) != -1 ){
            bool isOk;
            num = reg.cap(1).toInt(&isOk, 16 ) ;
            ui->pgb->setMaximum( num );
            isDownloadStart = true;
        }
    }else{
        qint32 index = txt.lastIndexOf( "flash_write_progress_async" );
        if( index == -1 ){
            return;
        }
        stm.seek(index);
        txt = stm.readLine();
        reg.setPattern( ":0x([0-9a-fA-F]+)\\|");
        if( reg.indexIn( txt ) != -1){
            bool isOk;
            num = reg.cap(1).toInt(&isOk, 16 );
            ui->pgb->setValue( num );
        }

        /* 结束判定 */
        if( num >= ui->pgb->maximum() ){
            isDownloadStart = false;
            isDownload = false;
        }
    }
}

/**
 * @brief Programmer::on_btnNetDownload_clicked 一键连接下载
 */
void Programmer::on_btnNetDownload_clicked()
{
    ui->btnNetDownload->setEnabled(false);
    emit ui->btnClearLog->clicked();
    delayMSec(20);

    /* 链接目标板子 */
    if( !isConnected ){
        ui->btnDeviceConnect->setChecked(true);
        emit ui->btnDeviceConnect->clicked(true);
    }

    while( !isConnected ){
        delayMSec(200);
    }

    /* 下载 */
    emit ui->btnDownload->clicked(true);

    quint32 i = 0;
    while(  (!isDownload) && (i++<10) ){
        delayMSec(100);// wait for download start
    }

    while( isDownload ){// wait for download complete
        delayMSec(500);
        if( !isConnected ){
            ui->btnNetDownload->setEnabled(true);
            return;
        }
    }
    ui->btnDeviceConnect->setChecked(false);
    emit ui->btnDeviceConnect->clicked(false);
    ui->btnNetDownload->setEnabled(true);
}

/**
 * @brief Programmer::on_tabOption_currentChanged 切换镜像文件
 * @param index
 */
void Programmer::on_tabOption_currentChanged(int index)
{
    if( index > 1 ){
        Memory *mem = qobject_cast<Memory *>(ui->tabOption->currentWidget()->children().at(0));
        ui->txtFilePath->setText( mem->getFilePath() );
    }
}

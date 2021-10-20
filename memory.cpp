#include "memory.h"
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QComboBox>
#include <QLayout>
#include <QMetaEnum>
#include <QDebug>
#include <QHeaderView>
#include <QtEndian>
#include <hextobin.h>


/**
 * @brief Memory::Memory 内存window 显示镜像数据
 * @param parent
 */
Memory::Memory(QWidget *parent) : QWidget(parent)
{
    btnRead      = new QPushButton();
    lblAddress   = new QLabel();
    spbAddress   = new QSpinBox();
    lblSize      = new QLabel();
    spbSize      = new QSpinBox();
    lblDataWidth = new QLabel();
    cbbDataWidth = new QComboBox();
    tblMemory    = new QTableWidget();

    btnRead->setText("Read");

    lblAddress->setText("Address:");
    spbAddress->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spbAddress->setMaximum(INT_MAX);
    spbAddress->setDisplayIntegerBase(16);
    spbAddress->setPrefix( "0x" );
    spbAddress->setValue( 0x08000000 );

    lblSize->setText("Size:");
    spbSize->setButtonSymbols(QAbstractSpinBox::NoButtons);
    spbSize->setMaximum(INT_MAX);
    spbSize->setDisplayIntegerBase(16);
    spbSize->setPrefix( "0x" );
    spbSize->setValue( 0x400 );

    lblDataWidth->setText("DataWidth");
    this->initDataWidth();

    QGridLayout *layout = new QGridLayout();
    layout->addWidget( btnRead,0, 0 );
    layout->addWidget( lblAddress,0, 1 );
    layout->addWidget( spbAddress,0, 2 );
    layout->addWidget( lblSize, 0, 3 );
    layout->addWidget( spbSize, 0, 4 );
    layout->addWidget( lblDataWidth, 0, 5 );
    layout->addWidget( cbbDataWidth, 0, 6 );
    layout->addWidget( tblMemory, 1,0,1,7);
    parent->setLayout(layout);

    connect( btnRead, &QPushButton::clicked, this, &Memory::signals_readMemory );
    connect( spbAddress, &QSpinBox::editingFinished, [=](){ address = spbAddress->value(); });
    connect( spbSize, &QSpinBox::editingFinished,    [=](){ size = spbSize->value(); });
    connect( cbbDataWidth, &QComboBox::currentTextChanged, [=](){
        QMetaEnum metaEnum = QMetaEnum::fromType<Memory::DataWidth>();
        datawidth = (DataWidth)metaEnum.keyToValue(cbbDataWidth->currentText().toLocal8Bit() );
    });

   /* not support write data */
    // TODO: 添加写功能
   tblMemory->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

Memory::~Memory()
{
    delete btnRead;
    delete lblAddress;
    delete spbAddress;
    delete lblSize;
    delete spbSize;
    delete lblDataWidth;
    delete cbbDataWidth;
    delete tblMemory;
    deleteLater();
}

/**
 * @brief Memory::initDataWidth 设置下拉菜单 'DataWidth'
 */
void Memory::initDataWidth()
{
    /* 枚举转stiring */
    QMetaEnum metaEnum = QMetaEnum::fromType<Memory::DataWidth>();
    QStringList list;
    qint32 count = metaEnum.keyCount();

    do{
        list << metaEnum.valueToKey( 1<<(count-1) );
        count--;
    }while(count);
    cbbDataWidth->addItems(list);
}
/**
 * @brief Memory::writeTable 将数据写入tablewidget
 * @param data
 */
void Memory::writeTable( QByteArray data )
{
    quint32 tblRow    = 0;
    quint32 addr      = address;
    qint32 memSize    = size;
    quint32 endAddr   = 0;

    if( data.size() < memSize )
        memSize = data.size();
    tblMemory->clear();
    data.resize( memSize );
    endAddr = addr + memSize;

    /* 调整行数量 */
    if( (memSize&0x0F) != 0 ){     // size % 16 != 0
        tblRow = (memSize>>4) + 1;
    }else {
        tblRow = memSize >>4;
    }    // size % 16 == 0

    tblMemory->setRowCount(tblRow);

    /* 调整行表头 */
    QStringList vHeaderList;
    while( addr < endAddr){
        vHeaderList<<("0x" + QString("%1").arg(addr,8,16,QLatin1Char('0')).toUpper());
        addr += 16;
    }
    tblMemory->setVerticalHeaderLabels(vHeaderList );

    /* 调整列数量 , 一行显示16个字节*/
    quint32 tblColumn = 0;
    tblMemory->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    QStringList hHeaderList;

    tblColumn = 16/datawidth;
    tblMemory->setColumnCount(tblColumn);
    /* 调整列表头 */
    for(quint8 i=0; i<16; i+=datawidth){
        hHeaderList << QString("%1").arg(i, 1, 16, QLatin1Char('0')).toUpper();
    }
    tblMemory->setHorizontalHeaderLabels(hHeaderList);

    this->insertDataInHex(tblMemory->rowCount(), tblMemory->columnCount(), data, datawidth);
}


/**
 * @brief Memory::writeDataInHex 转换成Hex字符并且显示在table上
 * @param row     行数量
 * @param column  列数量
 * @param array   数据内容
 * @param width   数据显示宽度
 */
void Memory::insertDataInHex( quint32 row, quint32 column, QByteArray &array, DataWidth width )
{
    /* 填充数据 */
    quint32 count = 0; // item的数量
    if( array.size()%width != 0 ){
        count = array.size() / width + 1;
        array.append(width, 0);
    } else {
        count = array.size() / width ;
    }

    /* 填充table->item */
    QTableWidgetItem *item;
    quint32 *intptr32 = (quint32 *)array.data();  // 数据的首地址
    quint16 *intptr16 = (quint16 *)array.data();
    quint8  *intptr8 = (quint8 *)array.data();
    for(quint32 i=0; i<row; i++){
        for(quint32 j=0; j<column; j++){
            item = tblMemory->item(i, j);
            if( item == NULL ){
                /* 添加item */
                item = new QTableWidgetItem();
                tblMemory->setItem(i, j, item);
            }

            /* 根据设定的数据宽度显示数据 */
            switch( width ){
            case Bits32:{
                quint32 tmp = qFromLittleEndian(*intptr32); // TODO:应先检测识别大小端
                item->setText(  QString("%1").arg(tmp, width*2, 16, QLatin1Char('0')).toUpper()  );
                intptr32++;
            }break;
            case Bits16:{
                quint16 tmp = qFromLittleEndian(*intptr16);
                item->setText(  QString("%1").arg(tmp, width*2, 16, QLatin1Char('0')).toUpper()  );
                intptr16++;
            }break;
            case Bits8:{
                quint16 tmp = qFromLittleEndian(*intptr8);
                item->setText(  QString("%1").arg(tmp, width*2, 16, QLatin1Char('0')).toUpper()  );
                intptr8++;
            }break;
            }
            count--;
            if( count == 0){
                return;
            }
        }
    }
}

/**
 * @brief Memory::writeBinFile 显示bin文件的内容
 * @param data
 */
void Memory::writeBinFile( QByteArray data )
{
    emit signals_msg(tr("Load file: %1\n").arg( this->filePath ) );
    emit signals_msg( tr("number of segment : 1\n") );
    emit signals_msg( tr("segment[0] : address = 0x0, size = 0x%1\n")
                      .arg(data.size(), 0, 16) );

    spbAddress->setEnabled( false );
    spbAddress->setValue(0);
    address = 0;
    writeTable(data);
}
/**
 * @brief Memory::writeHexFile 显示hex文件内容
 * @param data
 * @param firstAddr
 */
void Memory::writeHexFile( QByteArray data, quint32 firstAddr )
{
    spbAddress->setEnabled( false );
    spbAddress->setValue( firstAddr );
    address = firstAddr;
    writeTable(data);

}

/**
 * @brief Memory::writeHexFile
 * @param data
 * @param firstAddr
 */
void Memory::writeHexFile( QList<HexToBin::Segment*> segList )
{
    emit signals_msg(tr("Load file: %1\n").arg( this->filePath ) );
    tblMemory->setRowCount(0);
    address = segList.at(0)->address;
    spbAddress->setValue( address );
    spbAddress->setEnabled( false );

    quint32 size = 0;

    emit signals_msg( tr("number of segment : %1\n").arg( segList.count() ) );
    foreach (auto seg, segList) {
        insertTableItem(seg->address, seg->data);
        emit signals_msg( tr("segment[%1] : address = 0x%2, size = 0x%3\n")
                          .arg(segList.indexOf( seg))
                          .arg(seg->address, 0, 16)
                          .arg(seg->data.size(), 0, 16 ));
        size += seg->data.size();
    }
    spbSize->setValue( size );
}

/**
 * @brief Memory::insertTableItem 插入表格数据
 * @param addr 首地址
 * @param data 数据
 */
void Memory::insertTableItem( quint32 addr, QByteArray data)
{
    /* 调整列数量, 一行固定显示16个字节 */
    quint32 column = 0;
    QStringList hHeaderList;
    column = 16/datawidth;
    tblMemory->setColumnCount(column);
    /* 调整列表头 */
    for(quint8 i=0; i<16; i+=datawidth){
        hHeaderList << QString("%1").arg(i, 1, 16, QLatin1Char('0')).toUpper();
    }
    tblMemory->setHorizontalHeaderLabels(hHeaderList);
    tblMemory->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    /* 填充行------------------------- */
    QTableWidgetItem *cell;
    QTableWidgetItem *header = nullptr;
    quint32 endAddr = addr + data.size();

    quint32 *ptr32 = (quint32 *)data.data();  // 数据的首地址
    quint16 *ptr16 = (quint16 *)data.data();
    quint8  *ptr8  = (quint8  *)data.data();

    while(addr < endAddr){
        // 在底部添加新行
        qint32 currRow = tblMemory->rowCount();
        tblMemory->insertRow(currRow);
        header = tblMemory->verticalHeaderItem(currRow);
        if( header == nullptr){
            header = new QTableWidgetItem();
        }
        header->setText( "0x" + QString("%1").arg(addr, 8, 16, QLatin1Char('0')).toUpper());
        tblMemory->setVerticalHeaderItem(currRow, header);
        /* 填充这一行的数据 */
        for( quint32 i=0; i<column; i++ ){

            /* 获取或创建 item 指针 */
            cell = tblMemory->item(currRow, i);
            if( cell == NULL ){
                cell = new QTableWidgetItem();
                tblMemory->setItem(currRow, i, cell);
            }

            /* 填充数据 */
            switch( datawidth ){

            case Bits32:{
                quint32 tmp = qFromLittleEndian(*ptr32); // TODO:应先检测识别大小端
                cell->setText(  QString("%1").arg(tmp, datawidth*2, 16, QLatin1Char('0')).toUpper()  );
                ptr32++;
            }break;

            case Bits16:{
                quint16 tmp = qFromLittleEndian(*ptr16);
                cell->setText(  QString("%1").arg(tmp, datawidth*2, 16, QLatin1Char('0')).toUpper()  );
                ptr16++;
            }break;

            case Bits8:{
                quint16 tmp = qFromLittleEndian(*ptr8);
                cell->setText(  QString("%1").arg(tmp, datawidth*2, 16, QLatin1Char('0')).toUpper()  );
                ptr8++;
            }break;
            }
            if( (addr + i * datawidth) >= (endAddr-datawidth))
                break;
        }
        addr += 16;
    }
 }


/**
 * @brief Memory::getFilePath 读取文件路径
 * @return
 */
QString Memory::getFilePath() const
{
    return filePath;
}
/**
 * @brief Memory::setFilePath 设置文件路径
 * @param value
 */
void Memory::setFilePath(const QString &value)
{
    filePath = value;
}

/**
 * @brief Memory::clearItem 清空table 所有item
 */
void Memory::clearItem()
{
    tblMemory->clear();
    tblMemory->setRowCount(0);
}

#ifndef MEMORY_H
#define MEMORY_H

#include <QWidget>
#include "hextobin.h"

class QPushButton;
class QLabel;
class QSpinBox;
class QLabel;
class QSpinBox;
class QLabel;
class QComboBox;
class QTableWidget;
class HexToBin;
class Memory : public QWidget
{
    Q_OBJECT
public:
    explicit Memory(QWidget *parent = nullptr);
    ~Memory();

    enum DataWidth{
        Bits8  = 1,
        Bits16 = 2,
        Bits32 = 4,
    };
    Q_ENUM(DataWidth)

    QPushButton    *btnRead;
    QLabel         *lblAddress;
    QSpinBox       *spbAddress;
    QLabel         *lblSize;
    QSpinBox       *spbSize;
    QLabel         *lblDataWidth;
    QComboBox      *cbbDataWidth;
    QTableWidget   *tblMemory;
    void writeTable( QByteArray data );
    void writeBinFile( QByteArray data );
    void writeHexFile( QByteArray data, quint32 firstAddr );
    void writeHexFile( QList<HexToBin::Segment*> segList );


    QString getFilePath() const;
    void setFilePath(const QString &value);
    void loadFile(QString);
    void insertTableItem( quint32 addr, QByteArray data);
    void clearItem();

signals:
    void signals_readMemory( );
    void signals_msg( QString );

public slots:


private:
    void initDataWidth();

    quint32 address = 0x08000000; // default
    qint32 size = 0x400;         // default
    DataWidth datawidth = Bits32; // default
    void insertDataInHex( quint32 row, quint32 column, QByteArray &array, DataWidth width );
    QString filePath;
};

#endif // MEMORY_H

#ifndef HEXTOBIN_H
#define HEXTOBIN_H

#include <QObject>
#include <QList>

class QFile;
class HexToBin :public QObject
{
    enum RecordType{
        Data,
        EndOfFile,
        ExSegAddr,  // extened segment address
        StSegAddr,  // start segment address
        ExLineAddr, // extened linear address
        StLineAddr, // start linear address
    };

    struct HexLine{
        quint8      length;
        quint32     addr;
        quint8      recType;
        QByteArray  ptr;
        quint8      verifyNAdd8;
    };


public:
//    HexToBin();
    HexToBin(QFile *hexFile);
    ~HexToBin();

    struct Segment{
        quint32    address; // 首地址
        QByteArray data;    // 数据内容
    };


    bool isBinCorrect(){ return m_isCorrect; }
    QList<Segment*> segmentList(){return m_segList;} // hex seg data
    QByteArray segmentToBin( QList<Segment*> seg );  //  return bin array;


protected:
    bool isNewSeg;
    bool isVerifyOk(HexLine data);
    Segment *currentSeg;
    bool m_isCorrect = false;
    QList<Segment*> m_segList;
};

#endif // HEXTOBIN_H

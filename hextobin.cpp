#include "hextobin.h"
#include <QFile>
#include <QDebug>
#include <QList>

HexToBin::~HexToBin()
{

}
/**
 * @brief HexToBin::hextoBin 转换hex文件成bin文件
 * @param data
 * @return
 */
HexToBin::HexToBin(QFile *hexFile)
{
//    qDebug()<<hexFile->fileName();
    HexLine lineBinData;
    QByteArray lineHexData;
    quint32 currentAddr;

    while( !hexFile->atEnd() ){
        lineHexData = QByteArray::fromHex( hexFile->readLine());
        lineBinData.length  = (quint8)lineHexData.at(0);
        lineBinData.addr    = ((quint8)lineHexData.at(1)<<8) | (quint8)lineHexData.at(2);
        lineBinData.recType = (quint8)lineHexData.at(3);
        lineBinData.ptr = lineHexData.mid(4, lineBinData.length );
        lineBinData.verifyNAdd8 = lineHexData.at( lineHexData.size()-1 );
        lineHexData.clear();
        if(!this->isVerifyOk(lineBinData)){
            m_isCorrect = false;
            return;
        }

        switch( lineBinData.recType ){
        case Data:{

            if( isNewSeg == true ){
                isNewSeg = false;
                currentSeg->address += lineBinData.addr;
                currentAddr = currentSeg->address;
            }
            currentSeg->data.append( lineBinData.ptr );
            currentAddr += currentSeg->data.size();
        }break;

        case EndOfFile:
            break;

        case ExSegAddr:{
            Segment *newSeg = new Segment();
            m_segList<<newSeg;
            currentSeg = m_segList.last();
            currentSeg->address = lineBinData.ptr.toHex().toInt(bool(), 16) << 4;
            isNewSeg = true;
        }break;

//        case StSegAddr:
//            break;

        case ExLineAddr:{
            Segment *newSeg = new Segment();
            m_segList<<newSeg;
            currentSeg = m_segList.last();
            currentSeg->address = lineBinData.ptr.toHex().toInt(bool(), 16) << 16;
            isNewSeg = true;
        }break;
//        case StLineAddr:
//            break;
        }
    }
    m_isCorrect = true;
//    quint32 count = m_segList.count();
//    for( quint32 i=0; i<count; i++ ){
//        qDebug()<<"seg addr:"<< QString::number(m_segList.at(i)->address, 16);
//        qDebug()<<"seg size:"<< m_segList.at(i)->data.size();
//    }
    /* 先排序 */
    qSort(m_segList.begin(), m_segList.end(),
          [](const Segment *begin, const Segment *end){
            return begin->address < end->address;
          }
    );

}

/**
 * @brief HexToBin::isVerifyOk 计算校验
 * @param data
 * @return
 */
bool HexToBin::isVerifyOk(HexLine data)
{
    return true;
}
/**
 * @brief HexToBin::segmentToBin
 * @return
 */
QByteArray HexToBin::segmentToBin( QList<Segment*> seg )
{
    if( !m_isCorrect )
        return QByteArray();
    quint32 count = seg.count();
    QByteArray binArray;
    quint32 addrPtr = seg.at(0)->address; // 首地址 0x0800 0000
//    qDebug() << "convert to bin file" ;
    for(quint32 i=0; i<count; i++){
        if( addrPtr != seg.at(i)->address ){
            qint32 tmp = seg.at(i)->address - addrPtr;
            binArray.append(tmp, (char)0xFF);
            addrPtr += tmp;
        }
        binArray.append(seg.at(i)->data);
        addrPtr +=  seg.at(i)->data.size();
    }
    return binArray;
}

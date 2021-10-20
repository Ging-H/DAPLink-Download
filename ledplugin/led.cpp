#include "led.h"

Led::Led(QWidget *parent) :
    QRadioButton(parent)
{

    /* 初始状态 */
    this->setAutoExclusive(false);
    checked = qssChecked.arg(LED_ON_GREEN);
    unchecked = qssUnchecked.arg(LED_OFF_GREEN);
    setLedSize(QSize(defaultSize, defaultSize));
    this->setReadOnly(true);
}
Led::~Led()
{

}
/**
 * @brief Led::getLedOnColor LED Color when ON
 * @return
 */
Led::LedColor Led::getLedOnColor() const
{
    return ledOnColor;
}

/**
 * @brief Led::getLedOnColor set LED Color when ON
 * @return
 */
void Led::setLedOnColor(const LedColor &value)
{
    ledOnColor = value;
    switch( ledOnColor ){
    case White:
        checked = qssChecked.arg(LED_ON_WHITE);
        break;

    case Green:
        checked = qssChecked.arg(LED_ON_GREEN);
        break;

    case Red:
        checked = qssChecked.arg(LED_ON_RED);
        break;

    case Yellow:
        checked = qssChecked.arg(LED_ON_YELLOW);
        break;
    }

    /*  不可着色,使用单一颜色 */
    if( !isColorable ){
        switch( ledOnColor ){
        case White:
            unchecked = qssUnchecked.arg(LED_OFF_WHITE);
            break;

        case Green:
            unchecked = qssUnchecked.arg(LED_OFF_GREEN);
            break;

        case Red:
            unchecked = qssUnchecked.arg(LED_OFF_RED);
            break;

        case Yellow:
            unchecked = qssUnchecked.arg(LED_OFF_YELLOW);
            break;
        }
    }

    this->setStyleSheet( checked + size + unchecked);
}
/**
 * @brief Led::getLedSize 大小
 * @return
 */
QSize Led::getLedSize() const
{
    return ledSize;
}

/**
 * @brief Led::setLedSize 设置大小
 * @param value
 */
void Led::setLedSize(const QSize &value)
{
    ledSize = value;
    size = qssSize.arg(ledSize.width()).arg(ledSize.height());
    this->setStyleSheet( size + checked + unchecked);
}
/**
 * @brief Led::getLedOffColor LED Color when OFF
 * @return
 */
Led::LedColor Led::getLedOffColor() const
{
    return ledOffColor;
}
/**
 * @brief Led::setLedOffColor set LED Color when OFF
 * @param value
 */
void Led::setLedOffColor(const LedColor &value)
{
    ledOffColor = value;

    switch( ledOffColor ){
    case White:
        unchecked = qssUnchecked.arg(LED_OFF_WHITE);
        break;

    case Green:
        unchecked = qssUnchecked.arg(LED_OFF_GREEN);
        break;

    case Red:
        unchecked = qssUnchecked.arg(LED_OFF_RED);
        break;

    case Yellow:
        unchecked = qssUnchecked.arg(LED_OFF_YELLOW);
        break;
    }
    this->setStyleSheet( unchecked + size + checked);
}
/**
 * @brief Led::getColorable is LED Colorable
 * @return
 */
bool Led::getColorable() const
{
    return isColorable;
}
/**
 * @brief Led::setColorable set LED Colorable
 * @param value
 */
void Led::setColorable(bool value)
{
    isColorable = value;
}

bool Led::getReadOnly() const
{
    return isReadOnly;
}

void Led::setReadOnly(bool value)
{
    isReadOnly = value;
    if( isReadOnly ){
        this->setEnabled(false);
    }else{
        this->setEnabled(true);
    }
}

/**
 * @brief Led::resetSize reset size
 */
void Led::resetSize()
{
    ledSize.setHeight(defaultSize);
    ledSize.setWidth(defaultSize);
    size = qssSize.arg(defaultSize).arg(defaultSize);
    this->setStyleSheet( size + checked + unchecked);
}



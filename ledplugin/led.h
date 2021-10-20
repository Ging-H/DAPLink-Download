#ifndef LED_H
#define LED_H

#include <QRadioButton>
#include <QtUiPlugin/QDesignerExportWidget>

class QDESIGNER_WIDGET_EXPORT Led : public QRadioButton
{
    Q_OBJECT

    Q_PROPERTY(bool readonly READ getReadOnly WRITE setReadOnly )
    Q_PROPERTY(bool colorable READ getColorable WRITE setColorable )
    Q_PROPERTY(LedColor onColor READ getLedOnColor WRITE setLedOnColor)
    Q_PROPERTY(LedColor offColor READ getLedOffColor WRITE setLedOffColor DESIGNABLE getColorable)
    Q_PROPERTY(QSize size READ getLedSize WRITE setLedSize RESET resetSize)

#define LED_ON_YELLOW    ":/icon/led_on_yellow.png"
#define LED_ON_GREEN     ":/icon/led_on_green.png"
#define LED_ON_RED       ":/icon/led_on_red.png"
#define LED_ON_WHITE     ":/icon/led_on_white.png"

#define LED_OFF_YELLOW   ":/icon/led_off_yellow.png"
#define LED_OFF_GREEN    ":/icon/led_off_green.png"
#define LED_OFF_RED      ":/icon/led_off_red.png"
#define LED_OFF_WHITE    ":/icon/led_off_white.png"


public:
    Led(QWidget *parent = 0);
    ~Led();

    enum LedColor {
        White,
        Red,
        Green,
        Yellow,
    };
    Q_ENUM(LedColor)



    LedColor getLedOnColor() const;
    void setLedOnColor(const LedColor &value);

    QSize getLedSize() const;
    void setLedSize(const QSize &value);

    LedColor getLedOffColor() const;
    void setLedOffColor(const LedColor &value);

    bool getColorable() const;
    void setColorable(bool value);

    bool getReadOnly() const;
    void setReadOnly(bool value);

private:

    bool isReadOnly = true;
    bool isColorable = false;
    LedColor ledOnColor  = Green;
    LedColor ledOffColor = Green;
    QSize  ledSize;
    void resetSize();
    const qint32 defaultSize = 16;


/*RadioButton大小样式*/
    QString size ;
    const QString qssSize = "QRadioButton::indicator {width: %1px;height: %2px;}";
/*RadioButton未选中样式*/
    QString unchecked;
    const QString qssUnchecked = "QRadioButton::indicator::unchecked {image: url(%1);}";
/*RadioButton选中样式*/
    QString checked;
    const QString qssChecked = "QRadioButton::indicator::checked {image: url(%1);}";

};

#endif

#pragma once

#include "ui/icons.h"

#include <QColor>
#include <QFrame>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace mxu::ui {

class ColorButton : public QToolButton {
    Q_OBJECT
public:
    explicit ColorButton(QWidget *parent = nullptr);
    QColor color() const { return m_color; }
    void setColor(const QColor &color);

signals:
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QColor m_color = QColor(91, 141, 239);
};

class SegmentedControl : public QWidget {
    Q_OBJECT
public:
    explicit SegmentedControl(QWidget *parent = nullptr);
    void addItem(const QString &label, const QString &value);
    void setCurrentValue(const QString &value);
    QString currentValue() const;
    void setEnabledFor(const QString &value, bool enabled);

signals:
    void changed(const QString &value);

private:
    struct Item {
        QString value;
        QToolButton *button = nullptr;
    };
    QVector<Item> m_items;
    QString m_current;
    bool m_setting = false;
};

class TimerChip : public QFrame {
    Q_OBJECT
public:
    explicit TimerChip(QWidget *parent = nullptr);
    void setRemainingSeconds(int seconds);
    void setRunning(bool running);

signals:
    void toggleRequested();
    void resetRequested();
    void configureRequested();

private:
    QToolButton *m_time = nullptr;
    QToolButton *m_toggle = nullptr;
    bool m_running = false;
};

class SectionCard : public QFrame {
    Q_OBJECT
public:
    explicit SectionCard(const QString &title, QWidget *parent = nullptr);
    QVBoxLayout *body() const { return m_body; }

private:
    QVBoxLayout *m_body = nullptr;
};

QToolButton *makeIconButton(Glyph glyph, const QString &tooltip, QWidget *parent = nullptr);
QLabel *makeLabel(const QString &text, const QString &objectName = QString(), QWidget *parent = nullptr);
QFrame *makeSeparator(QWidget *parent = nullptr);

} // namespace mxu::ui

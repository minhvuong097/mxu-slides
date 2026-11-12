#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

namespace mxu::ui {

struct Palette {
    QColor window;
    QColor panel;
    QColor surface;
    QColor surfaceRaised;
    QColor border;
    QColor borderStrong;
    QColor text;
    QColor textMuted;
    QColor textFaint;
    QColor accent;
    QColor accentHover;
    QColor accentSoft;
    QColor live;
    QColor warning;
    QColor success;
    QColor shadow;
};

const Palette &palette();
void applyTheme(QApplication &application);
QFont uiFont(int pointSize = 10, QFont::Weight weight = QFont::Normal);
QString formatTimer(int seconds);

} // namespace mxu::ui

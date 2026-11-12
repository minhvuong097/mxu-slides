#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace mxu::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const QString &libraryRoot, const QString &outputScreen,
                            bool stageEnabled, const QString &stageScreen, int timerSeconds,
                            QWidget *parent = nullptr);

    QString libraryRoot() const;
    QString outputScreen() const;
    bool stageEnabled() const;
    QString stageScreen() const;
    int timerSeconds() const;

private:
    void browseLibrary();

    QLineEdit *m_libraryRoot = nullptr;
    QComboBox *m_outputScreen = nullptr;
    QCheckBox *m_stageEnabled = nullptr;
    QComboBox *m_stageScreen = nullptr;
    QSpinBox *m_timerMinutes = nullptr;
};

} // namespace mxu::ui

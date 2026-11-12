#pragma once

#include "core/document.h"

#include <QWidget>

#include <functional>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QWidget;

namespace mxu {
class Editor;
class SlideRenderer;
}

namespace mxu::ui {

class ColorButton;
class SegmentedControl;

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(Editor *editor, SlideRenderer *renderer, QWidget *parent = nullptr);

    void refresh();
    void flushPendingEdits();
    void focusTextEditor();

private:
    void buildSlidePage();
    void buildObjectPage();
    void buildDeckPage();
    void updateSlideFields();
    void updateObjectFields();
    void updateDeckFields();
    void updateSelectionTab();
    void updateFontFamilies();

    void mutateSlide(const QString &label, const std::function<void(Slide &)> &edit);
    void mutateObject(const QString &label, const std::function<void(SlideObject &)> &edit);
    void mutateDeck(const QString &label, const std::function<void(Presentation &)> &edit);

    void applyPendingText();
    void applyPendingNotes();
    void scheduleTextEdit();
    void scheduleNotesEdit();
    void chooseObjectMedia();
    void clearObjectMedia();
    void editTheme();
    void resetSlideBackground();
    void syncFillControls();

    Editor *m_editor = nullptr;
    SlideRenderer *m_renderer = nullptr;
    QTabWidget *m_tabs = nullptr;

    QLineEdit *m_slideName = nullptr;
    ColorButton *m_slideBackground = nullptr;
    QPushButton *m_slideBackgroundReset = nullptr;
    QPlainTextEdit *m_notes = nullptr;
    QCheckBox *m_transitionEnabled = nullptr;
    QComboBox *m_transitionKind = nullptr;
    QDoubleSpinBox *m_transitionDuration = nullptr;
    QPushButton *m_deleteSlide = nullptr;

    QStackedWidget *m_objectStack = nullptr;
    QLineEdit *m_objectName = nullptr;
    QLabel *m_objectType = nullptr;
    QPlainTextEdit *m_objectText = nullptr;
    QComboBox *m_fontFamily = nullptr;
    QDoubleSpinBox *m_fontSize = nullptr;
    ColorButton *m_textColor = nullptr;
    QCheckBox *m_textColorSet = nullptr;
    SegmentedControl *m_hAlignment = nullptr;
    SegmentedControl *m_vAlignment = nullptr;
    QCheckBox *m_autoShrink = nullptr;

    QWidget *m_textSection = nullptr;
    QWidget *m_shapeSection = nullptr;
    QComboBox *m_shapeKind = nullptr;
    QDoubleSpinBox *m_cornerRadius = nullptr;

    QWidget *m_fillSection = nullptr;
    QFormLayout *m_fillForm = nullptr;
    QComboBox *m_fillKind = nullptr;
    QWidget *m_fillColorRow = nullptr;
    QWidget *m_gradientRow = nullptr;
    ColorButton *m_fillColor = nullptr;
    ColorButton *m_gradientColor = nullptr;
    QDoubleSpinBox *m_gradientAngle = nullptr;

    QWidget *m_mediaSection = nullptr;
    QLabel *m_mediaName = nullptr;
    QComboBox *m_mediaScale = nullptr;

    QCheckBox *m_strokeEnabled = nullptr;
    ColorButton *m_strokeColor = nullptr;
    QDoubleSpinBox *m_strokeWidth = nullptr;

    QCheckBox *m_shadowEnabled = nullptr;
    ColorButton *m_shadowColor = nullptr;
    QDoubleSpinBox *m_shadowBlur = nullptr;
    QDoubleSpinBox *m_shadowX = nullptr;
    QDoubleSpinBox *m_shadowY = nullptr;

    QDoubleSpinBox *m_frameX = nullptr;
    QDoubleSpinBox *m_frameY = nullptr;
    QDoubleSpinBox *m_frameWidth = nullptr;
    QDoubleSpinBox *m_frameHeight = nullptr;
    QDoubleSpinBox *m_rotation = nullptr;
    QDoubleSpinBox *m_opacity = nullptr;

    QPushButton *m_duplicateObject = nullptr;
    QPushButton *m_forwardObject = nullptr;
    QPushButton *m_backwardObject = nullptr;
    QPushButton *m_deleteObject = nullptr;

    QLineEdit *m_deckName = nullptr;
    QComboBox *m_canvasSize = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QLabel *m_themeDetails = nullptr;
    QPushButton *m_editTheme = nullptr;

    QTimer *m_textTimer = nullptr;
    QTimer *m_notesTimer = nullptr;
    QString m_pendingObjectId;
    QString m_pendingText;
    int m_pendingObjectSlide = -1;
    QString m_pendingNotes;
    QString m_pendingNotesSlideId;

    QString m_lastSelectedId;
    bool m_updating = false;
    bool m_fontsReady = false;
};

} // namespace mxu::ui

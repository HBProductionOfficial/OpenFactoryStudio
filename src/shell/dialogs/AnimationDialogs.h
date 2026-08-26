#pragma once

#include "core/anims/AirDocument.h"
#include "core/anims/AirEdits.h"

#include <QDialog>

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QSlider;
class QSpinBox;

namespace OFS {

class CodeEditor;

class AddAnimationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddAnimationDialog(QWidget* parent = nullptr);
    QString name() const;
    int number() const;
    int time() const;

private:
    QLineEdit* m_name;
    QSpinBox* m_number;
    QSpinBox* m_time;
};

// Animations > Alignment Tool. Each Apply is one undo step and the dialog stays open.
class AnimationAlignDialog : public QDialog
{
    Q_OBJECT

public:
    AnimationAlignDialog(const AirDocument& doc, int current, QWidget* parent = nullptr);

Q_SIGNALS:
    void applyRequested(const QList<int>& animations, AirEdits::AlignOperation operation, int x, int y);

private:
    QComboBox* m_operation;
    QSpinBox* m_x;
    QSpinBox* m_y;
    QListWidget* m_list;
};

// Save as animated GIF: the scale (0 = half) and the speed.
class GifOptionsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GifOptionsDialog(QWidget* parent = nullptr);
    double scale() const;
    double velocity() const;

private:
    QLabel* m_label;
    QSlider* m_scale;
    QDoubleSpinBox* m_velocity;
};

// Animations > Edit animation code. Apply keeps the window open, and closing with changes not applied asks first.
class AnimationCodeWindow : public QDialog
{
    Q_OBJECT

public:
    AnimationCodeWindow(const QStringList& text, QWidget* parent = nullptr);
    CodeEditor* editor() const { return m_editor; }
    QStringList text() const;
    bool isModified() const;

Q_SIGNALS:
    void applyRequested(const QStringList& text);

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    bool confirmClose();

    CodeEditor* m_editor;
};

} // namespace OFS

#pragma once

#include "engines/mugen/MugenHtmlExport.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStandardItemModel;
class QWidget;

namespace OFS::Mugen {

class ExportHtmlDialog : public QDialog
{
    Q_OBJECT

public:
    ExportHtmlDialog(const QList<QPair<QString, QString>>& layouts, bool stage, const QList<HtmlCommand>& commands,
                     QWidget* parent = nullptr);

    HtmlExportOptions options() const;

private:
    void updateRendering();
    void setBackground(const QColor& color);

    bool m_stage;
    QList<QPair<QString, QString>> m_layouts;
    QComboBox* m_layout;
    QLineEdit* m_mail;
    QLineEdit* m_site;
    QLineEdit* m_title;
    QPlainTextEdit* m_text;
    QCheckBox* m_hline;
    QCheckBox* m_bold;
    QStandardItemModel* m_commands;
    QCheckBox* m_emulator;
    QPushButton* m_backgroundButton;
    QWidget* m_backgroundSwatch;
    QColor m_background = Qt::black;
    QSpinBox* m_frameStart;
    QSpinBox* m_duration;
    QSpinBox* m_fps;
    QCheckBox* m_sharedPalette;
    QGroupBox* m_staticSize = nullptr;
    QSpinBox* m_staticWidth = nullptr;
    QSpinBox* m_staticHeight = nullptr;
    QGroupBox* m_gifSize;
    QSpinBox* m_gifWidth;
    QSpinBox* m_gifHeight;
};

} // namespace OFS::Mugen

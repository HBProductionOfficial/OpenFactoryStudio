#pragma once

#include "core/CoreExport.h"

#include <QFrame>

class QLabel;
class QVBoxLayout;

// Side panel with a title strip on top. It sits outside the OFS namespace because interface style
// sheets match the C++ class name "EPanelWidget".
class OFS_CORE_EXPORT EPanelWidget : public QFrame
{
    Q_OBJECT
    // Set by style sheets (qproperty-leftMargin...).
    Q_PROPERTY(int leftMargin READ leftMargin WRITE setLeftMargin)
    Q_PROPERTY(int topMargin READ topMargin WRITE setTopMargin)
    Q_PROPERTY(int rightMargin READ rightMargin WRITE setRightMargin)
    Q_PROPERTY(int bottomMargin READ bottomMargin WRITE setBottomMargin)

public:
    explicit EPanelWidget(QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& title);

    QVBoxLayout* bodyLayout() const { return m_body; }
    // Object name "panelContent".
    QWidget* content() const { return m_content; }

    int leftMargin() const;
    int topMargin() const;
    int rightMargin() const;
    int bottomMargin() const;
    void setLeftMargin(int margin);
    void setTopMargin(int margin);
    void setRightMargin(int margin);
    void setBottomMargin(int margin);

private:
    QVBoxLayout* m_layout;
    QLabel* m_title;
    QWidget* m_content;
    QVBoxLayout* m_body;
};

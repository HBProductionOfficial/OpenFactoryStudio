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

public:
    explicit EPanelWidget(QWidget* parent = nullptr);

    QString title() const;
    void setTitle(const QString& title);

    QVBoxLayout* bodyLayout() const { return m_body; }

private:
    QLabel* m_title;
    QVBoxLayout* m_body;
};

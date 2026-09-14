#include "shell/WelcomePage.h"

#include "core/settings/History.h"

#include <QCommandLinkButton>
#include <QDir>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace OFS {

namespace {

QLabel* heading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("welcomeHeading"));
    QFont f = label->font();
    f.setPointSizeF(f.pointSizeF() * 1.35);
    label->setFont(f);
    return label;
}

QLabel* paragraph(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("welcomeText"));
    label->setWordWrap(true);
    return label;
}

} // namespace

WelcomePage::WelcomePage(History& history, QWidget* parent)
    : QWidget(parent)
    , m_history(history)
    , m_grid(new QGridLayout)
    , m_edit(new QToolButton(this))
    , m_remove(new QToolButton(this))
{
    setObjectName(QStringLiteral("welcomePage"));

    // Left column: introduction and project links.
    auto* left = new QWidget(this);
    left->setFixedWidth(295);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);
    leftLayout->addWidget(heading(tr("New to Open Factory Studio?"), left));
    leftLayout->addWidget(
        paragraph(tr("Learn how to create characters, stages and screenpacks and explore Open Factory Studio."), left));
    auto* getStarted = new QPushButton(tr("GET STARTED NOW"), left);
    getStarted->setObjectName(QStringLiteral("welcomeGetStarted"));
    leftLayout->addWidget(getStarted);
    leftLayout->addSpacing(12);
    leftLayout->addWidget(heading(tr("Join the project"), left));
    leftLayout->addWidget(paragraph(tr("Open Factory Studio is free and open source. Report problems, suggest "
                                       "features or contribute code and translations."),
                                    left));
    auto* contribute = new QPushButton(tr("CONTRIBUTE"), left);
    contribute->setObjectName(QStringLiteral("welcomeContribute"));
    leftLayout->addWidget(contribute);
    leftLayout->addStretch(1);
    connect(getStarted, &QPushButton::clicked, this, &WelcomePage::getStartedRequested);
    connect(contribute, &QPushButton::clicked, this, &WelcomePage::contributeRequested);

    auto* divider = new QFrame(this);
    divider->setFrameShape(QFrame::VLine);
    divider->setObjectName(QStringLiteral("welcomeDivider"));

    // Right area: recent projects.
    auto* right = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto* header = new QHBoxLayout;
    header->addWidget(heading(tr("Recent Projects"), right));
    header->addStretch(1);
    m_remove->setText(tr("Remove selected"));
    m_remove->setObjectName(QStringLiteral("welcomeRemove"));
    m_remove->setAutoRaise(true);
    m_remove->hide();
    m_edit->setText(tr("Edit"));
    m_edit->setObjectName(QStringLiteral("welcomeEdit"));
    m_edit->setCheckable(true);
    m_edit->setAutoRaise(true);
    header->addWidget(m_remove);
    header->addWidget(m_edit);
    rightLayout->addLayout(header);

    auto* gridHost = new QWidget(right);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(12);
    m_grid->setVerticalSpacing(12);
    gridHost->setLayout(m_grid);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(gridHost);
    rightLayout->addWidget(scroll, 1);

    connect(m_edit, &QToolButton::toggled, this, &WelcomePage::setEditing);
    connect(m_remove, &QToolButton::clicked, this, &WelcomePage::removeSelected);
    connect(&m_history, &History::changed, this, &WelcomePage::refresh);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(33, 33, 33, 0);
    layout->setSpacing(30);
    layout->addWidget(left);
    layout->addWidget(divider);
    layout->addWidget(right, 1);

    refresh();
}

bool WelcomePage::isEditing() const
{
    return m_edit->isChecked();
}

void WelcomePage::refresh()
{
    for (QCommandLinkButton* b : std::as_const(m_buttons))
        b->deleteLater();
    m_buttons.clear();

    const auto& entries = m_history.entries();
    for (qsizetype i = 0; i < entries.size(); ++i) {
        const History::Entry& e = entries[i];
        auto* button = new QCommandLinkButton(e.name, QDir::toNativeSeparators(e.file), this);
        button->setObjectName(QStringLiteral("recentProject%1").arg(i + 1));
        button->setToolTip(QDir::toNativeSeparators(e.file));
        button->setCheckable(isEditing());
        button->setProperty("file", e.file);
        button->setMinimumHeight(50);
        connect(button, &QCommandLinkButton::clicked, this, [this, button] {
            if (!isEditing())
                Q_EMIT openProjectRequested(button->property("file").toString());
        });
        m_grid->addWidget(button, int(i / 2), int(i % 2));
        m_buttons << button;
    }
    if (entries.isEmpty()) {
        auto* open = new QCommandLinkButton(tr("Open project..."),
                                            tr("No recent projects. Open one now and start the fun!"), this);
        open->setObjectName(QStringLiteral("welcomeOpenProject"));
        open->setMinimumHeight(50);
        connect(open, &QCommandLinkButton::clicked, this, &WelcomePage::openDialogRequested);
        m_grid->addWidget(open, 0, 0, 1, 2);
        m_buttons << open;
    }
    m_edit->setEnabled(!entries.isEmpty());
    m_grid->setRowStretch(int((qMax<qsizetype>(entries.size(), 1) + 1) / 2), 1);
}

void WelcomePage::setEditing(bool editing)
{
    m_remove->setVisible(editing);
    for (QCommandLinkButton* b : std::as_const(m_buttons)) {
        b->setChecked(false);
        b->setCheckable(editing);
    }
}

void WelcomePage::removeSelected()
{
    QStringList files;
    for (QCommandLinkButton* b : std::as_const(m_buttons)) {
        if (b->isChecked())
            files << b->property("file").toString();
    }
    for (const QString& f : files)
        m_history.remove(f);
    m_edit->setChecked(false);
}

} // namespace OFS

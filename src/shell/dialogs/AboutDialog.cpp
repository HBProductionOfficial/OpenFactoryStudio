#include "shell/dialogs/AboutDialog.h"

#include "core/Version.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPixmap>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace OFS {

namespace {

QString readResource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll());
}

// Third party notices.pdf comes with the program: next to it on Windows, in Resources on macOS and in
// share/doc on Linux. A build folder has none, so the link goes to the project page there.
QString noticesLink()
{
    const QDir app(QCoreApplication::applicationDirPath());
    const QString name = QStringLiteral("Third party notices.pdf");
    for (const QString& folder :
         {QStringLiteral("."), QStringLiteral("../Resources"), QStringLiteral("../share/doc/OpenFactoryStudio")}) {
        const QFileInfo file(app.filePath(folder + QLatin1Char('/') + name));
        if (file.isFile())
            return QUrl::fromLocalFile(file.absoluteFilePath()).toString();
    }
    return QStringLiteral(OFS_HOMEPAGE);
}

QTextBrowser* createBrowser(QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setOpenExternalLinks(true);
    browser->setFrameShape(QFrame::NoFrame);
    return browser;
}

} // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
    , m_tabs(new QTabWidget(this))
    , m_legal(createBrowser(m_tabs))
    , m_licenses(createBrowser(m_tabs))
{
    setObjectName(QStringLiteral("AboutDlg"));
    setWindowTitle(tr("About..."));

    auto* icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("icon"));
    icon->setFixedSize(75, 66);
    icon->setAlignment(Qt::AlignCenter);
    QPixmap pixmap(QStringLiteral(":/ofs/data/icons/ofs.png"));
    if (!pixmap.isNull()) {
        const qreal ratio = devicePixelRatioF();
        pixmap = pixmap.scaled(QSize(64, 64) * ratio, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(ratio);
        icon->setPixmap(pixmap);
    }

    auto* name = new QLabel(QStringLiteral(OFS_DISPLAY_NAME), this);
    name->setObjectName(QStringLiteral("name"));
    QFont bold = name->font();
    bold.setBold(true);
    name->setFont(bold);
    auto* version = new QLabel(versionText(), this);
    version->setObjectName(QStringLiteral("ver"));
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* copyright = new QLabel(tr("Copyright © 2026 the Open Factory Studio contributors."), this);
    copyright->setObjectName(QStringLiteral("copyright"));

    m_tabs->setObjectName(QStringLiteral("tabWidget"));
    m_legal->setObjectName(QStringLiteral("legal"));
    m_licenses->setObjectName(QStringLiteral("licenses"));
    m_tabs->addTab(m_legal, tr("Legal information"));
    m_tabs->addTab(m_licenses, tr("Licenses"));

    const QString homepage = QStringLiteral(OFS_HOMEPAGE);
    m_legal->setHtml(
        tr("<p>%1 is free software: you can redistribute it and/or modify it under the terms of the GNU General "
           "Public License version 3 (GPL-3.0) as published by the Free Software Foundation.</p>"
           "<p>The source code is available at <a href=\"%2\">%2</a>.</p>"
           "<p>The program is built with Qt, licensed under the GNU LGPL version 3, and runs presets and scripts "
           "with Lua through sol2, both licensed under the MIT license. Third party notices.pdf, which comes with "
           "the program, lists every part with its license.</p>"
           "<p>M.U.G.E.N is a product of Elecbyte. %1 is an independent project and is not affiliated with or "
           "endorsed by Elecbyte.</p>"
           "<p>The program is provided AS IS, WITHOUT WARRANTY OF ANY KIND; see the GNU General Public License "
           "for details.</p>")
            .arg(QStringLiteral(OFS_DISPLAY_NAME), homepage));

    QString licenses = tr("<p>%1 uses Qt, Lua, sol2 and other open source parts. Their licenses are in <a "
                          "href=\"%2\">Third party notices.pdf</a>, which comes with the program.</p>")
                           .arg(QStringLiteral(OFS_DISPLAY_NAME), noticesLink());
    const QString gpl = licenseText();
    if (!gpl.isEmpty())
        licenses += QStringLiteral("<h3>GNU General Public License v3.0</h3><pre>") + gpl.toHtmlEscaped() +
                    QStringLiteral("</pre>");
    m_licenses->setHtml(licenses);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    buttons->setObjectName(QStringLiteral("buttonBox"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* header = new QVBoxLayout;
    header->setSpacing(6);
    header->addWidget(name);
    header->addWidget(version);
    header->addWidget(copyright);
    header->addStretch(1);

    auto* grid = new QGridLayout(this);
    grid->addWidget(icon, 0, 0, Qt::AlignTop);
    grid->addLayout(header, 0, 1);
    grid->addWidget(m_tabs, 1, 0, 1, 2);
    grid->addWidget(buttons, 2, 0, 1, 2);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(1, 1);

    resize(606, 634);
}

void AboutDialog::showAbout(QWidget* parent)
{
    AboutDialog dialog(parent);
    dialog.exec();
}

QString AboutDialog::versionText()
{
    return QStringLiteral("%1 (Qt %2)").arg(QStringLiteral(OFS_VERSION_STRING), QString::fromLatin1(qVersion()));
}

QString AboutDialog::licenseText()
{
    return readResource(QStringLiteral(":/ofs/LICENSE"));
}

} // namespace OFS

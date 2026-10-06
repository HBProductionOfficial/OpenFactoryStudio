#include "core/widgets/RenderSurface.h"

#include "core/settings/Config.h"

#include <QDir>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QPainter>

namespace OFS::Rendering {

namespace {

Backend g_backend = Backend::Native;
QString g_guard; // exists until an OpenGL start draws its first frame

bool hasOpenGL2()
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QLatin1String("offscreen") || platform == QLatin1String("minimal"))
        return false;
    QOpenGLContext context;
    if (!context.create())
        return false;
    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid() || !context.makeCurrent(&surface))
        return false;
    const bool ok = context.format().majorVersion() >= 2;
    context.doneCurrent();
    return ok;
}

class Surface final : public QOpenGLWidget
{
public:
    Surface(QWidget* owner, std::function<void(QPainter&)> paint)
        : QOpenGLWidget(owner)
        , m_paint(std::move(paint))
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(owner->rect());
        owner->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parent() && event->type() == QEvent::Resize)
            setGeometry(parentWidget()->rect());
        return false;
    }

    void paintGL() override
    {
        {
            QPainter p(this);
            m_paint(p);
        }
        if (!g_guard.isEmpty()) {
            QFile::remove(g_guard);
            g_guard.clear();
        }
    }

private:
    std::function<void(QPainter&)> m_paint;
};

} // namespace

void choose(const Config& config, const QString& userDir)
{
    g_backend = Backend::Native;
    if (config.boolValue(QStringLiteral("native_painting"), false))
        return;
    const QString guard = QDir(userDir).filePath(QStringLiteral("renderer.lock"));
    if (QFile::exists(guard)) {
        // The last start with OpenGL ended before its first frame.
        QFile::remove(guard);
        return;
    }
    if (!hasOpenGL2())
        return;
    QFile mark(guard);
    if (mark.open(QIODevice::WriteOnly)) {
        mark.write("OpenGL\n");
        g_guard = guard;
    }
    g_backend = Backend::OpenGL;
}

void finish()
{
    if (!g_guard.isEmpty()) {
        QFile::remove(g_guard);
        g_guard.clear();
    }
}

Backend backend()
{
    return g_backend;
}

QString backendName()
{
    return g_backend == Backend::OpenGL ? QStringLiteral("OpenGL") : QStringLiteral("Native");
}

QWidget* attachSurface(QWidget* owner, std::function<void(QPainter&)> paint)
{
    if (g_backend != Backend::OpenGL || !owner)
        return nullptr;
    return new Surface(owner, std::move(paint));
}

void update(QWidget* surface)
{
    if (surface)
        surface->update();
}

void prepareWindow(QWidget* window)
{
    if (g_backend != Backend::OpenGL || !window)
        return;
    // A hidden OpenGL widget makes Qt create the window the OpenGL way from the start.
    auto* keeper = new QOpenGLWidget(window);
    keeper->setObjectName(QStringLiteral("openGLWindowKeeper"));
    keeper->setAttribute(Qt::WA_TransparentForMouseEvents);
    keeper->setFocusPolicy(Qt::NoFocus);
    keeper->setGeometry(0, 0, 0, 0);
    keeper->hide();
}

} // namespace OFS::Rendering

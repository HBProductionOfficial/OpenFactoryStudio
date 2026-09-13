#include "shell/TaskbarTabs.h"

#include <QGuiApplication>
#include <QWidget>

#if defined(Q_OS_WIN)
#  include <QAbstractNativeEventFilter>
#  include <QPointer>

#  include <algorithm>

#  include <dwmapi.h>
#  include <shobjidl.h>
#  include <windows.h>
#endif

namespace OFS {

#if defined(Q_OS_WIN)

namespace {

// Hidden window standing for one tab in the taskbar. The taskbar asks it for its pictures and tells it when its
// thumbnail is clicked or closed.
class Proxy : public QWidget
{
public:
    Proxy(TaskbarTabs* owner, QWidget* window, QObject* key)
        : QWidget(nullptr, Qt::Window)
        , m_owner(owner)
        , m_window(window)
        , m_key(key)
    {
        setWindowIcon(window->windowIcon());
        const BOOL on = TRUE;
        DwmSetWindowAttribute(hwnd(), DWMWA_FORCE_ICONIC_REPRESENTATION, &on, sizeof on);
        DwmSetWindowAttribute(hwnd(), DWMWA_HAS_ICONIC_BITMAP, &on, sizeof on);
    }

    HWND hwnd() { return reinterpret_cast<HWND>(winId()); }
    QObject* key() const { return m_key; }
    void setPortrait(std::function<QImage()> portrait) { m_portrait = std::move(portrait); }

protected:
    bool nativeEvent(const QByteArray& type, void* message, qintptr* result) override
    {
        Q_UNUSED(type);
        const MSG* msg = static_cast<const MSG*>(message);
        switch (msg->message) {
        case WM_ACTIVATE:
            if (LOWORD(msg->wParam) != WA_INACTIVE) {
                Q_EMIT m_owner->activated(m_key);
                *result = 0;
                return true;
            }
            break;
        case WM_SYSCOMMAND:
            if ((msg->wParam & 0xfff0) != SC_CLOSE)
                break;
            [[fallthrough]];
        case WM_CLOSE:
            Q_EMIT m_owner->closeRequested(m_key);
            *result = 0;
            return true;
        case WM_DWMSENDICONICTHUMBNAIL:
            sendThumbnail(QSize(HIWORD(msg->lParam), LOWORD(msg->lParam)));
            *result = 0;
            return true;
        case WM_DWMSENDICONICLIVEPREVIEWBITMAP:
            sendLivePreview();
            *result = 0;
            return true;
        default:
            break;
        }
        return false;
    }

private:
    void sendThumbnail(const QSize& maximum)
    {
        QImage picture = m_portrait ? m_portrait() : QImage();
        if (picture.isNull() && m_window)
            picture = m_window->grab().toImage();
        if (picture.isNull() || maximum.isEmpty())
            return;
        // Do not blow up small portraits. The bitmap must not be bigger than the taskbar asked for.
        if (picture.width() > maximum.width() || picture.height() > maximum.height())
            picture = picture.scaled(maximum, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const HBITMAP bitmap = picture.convertToFormat(QImage::Format_ARGB32_Premultiplied).toHBITMAP();
        if (!bitmap)
            return;
        DwmSetIconicThumbnail(hwnd(), bitmap, 0);
        DeleteObject(bitmap);
    }

    void sendLivePreview()
    {
        if (!m_window)
            return;
        const QImage picture = m_window->grab().toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const HBITMAP bitmap = picture.toHBITMAP();
        if (!bitmap)
            return;
        RECT frame;
        POINT client{0, 0};
        const HWND main = reinterpret_cast<HWND>(m_window->winId());
        GetWindowRect(main, &frame);
        ClientToScreen(main, &client);
        POINT offset{client.x - frame.left, client.y - frame.top};
        DwmSetIconicLivePreviewBitmap(hwnd(), bitmap, &offset, DWM_SIT_DISPLAYFRAME);
        DeleteObject(bitmap);
    }

    TaskbarTabs* m_owner;
    QPointer<QWidget> m_window;
    QObject* m_key;
    std::function<QImage()> m_portrait;
};

UINT taskbarButtonCreated()
{
    static const UINT message = RegisterWindowMessageW(L"TaskbarButtonCreated");
    return message;
}

} // namespace

struct TaskbarTabs::Private : QAbstractNativeEventFilter
{
    TaskbarTabs* q = nullptr;
    QWidget* window = nullptr;
    ITaskbarList3* taskbar = nullptr;
    bool buttonReady = false;
    QList<Tab> tabs;
    QObject* current = nullptr;
    QList<Proxy*> proxies;

    ~Private() override
    {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        clear();
        if (taskbar)
            taskbar->Release();
    }

    HWND mainWindow() const { return reinterpret_cast<HWND>(window->winId()); }

    void clear()
    {
        for (Proxy* proxy : std::as_const(proxies)) {
            if (taskbar && buttonReady)
                taskbar->UnregisterTab(proxy->hwnd());
            delete proxy;
        }
        proxies.clear();
    }

    // The taskbar made the window's button, or made it again after Explorer restarted. From now on the tabs can be
    // registered.
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override
    {
        Q_UNUSED(type);
        Q_UNUSED(result);
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message == taskbarButtonCreated() && window->internalWinId() &&
            msg->hwnd == reinterpret_cast<HWND>(window->internalWinId())) {
            if (!taskbar &&
                (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))) ||
                 FAILED(taskbar->HrInit()))) {
                if (taskbar)
                    taskbar->Release();
                taskbar = nullptr;
                return false;
            }
            // Register every tab again because the old buttons went with the old taskbar.
            clear();
            buttonReady = true;
            apply();
        }
        return false;
    }

    void apply()
    {
        if (!taskbar || !buttonReady)
            return;
        for (qsizetype i = proxies.size() - 1; i >= 0; --i) {
            Proxy* proxy = proxies[i];
            const bool kept =
                std::any_of(tabs.cbegin(), tabs.cend(), [proxy](const Tab& tab) { return tab.key == proxy->key(); });
            if (!kept) {
                taskbar->UnregisterTab(proxy->hwnd());
                delete proxies.takeAt(i);
            }
        }
        QList<Proxy*> ordered;
        Proxy* active = nullptr;
        for (const Tab& tab : std::as_const(tabs)) {
            auto found =
                std::find_if(proxies.begin(), proxies.end(), [&tab](Proxy* proxy) { return proxy->key() == tab.key; });
            Proxy* proxy = found != proxies.end() ? *found : nullptr;
            if (!proxy) {
                proxy = new Proxy(q, window, tab.key);
                taskbar->RegisterTab(proxy->hwnd(), mainWindow());
                proxies << proxy;
            }
            proxy->setWindowTitle(tab.title);
            proxy->setPortrait(tab.portrait);
            ordered << proxy;
            if (tab.key == current)
                active = proxy;
        }
        for (Proxy* proxy : std::as_const(ordered)) {
            taskbar->SetTabOrder(proxy->hwnd(), nullptr);
            DwmInvalidateIconicBitmaps(proxy->hwnd());
        }
        if (active)
            taskbar->SetTabActive(active->hwnd(), mainWindow(), 0);
        proxies = ordered;
    }
};

TaskbarTabs::TaskbarTabs(QWidget* window)
    : QObject(window)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->window = window;
    if (isSupported())
        QCoreApplication::instance()->installNativeEventFilter(d.get());
}

TaskbarTabs::~TaskbarTabs() = default;

bool TaskbarTabs::isSupported()
{
    return QGuiApplication::platformName() == QLatin1String("windows");
}

void TaskbarTabs::setTabs(const QList<Tab>& tabs, QObject* current)
{
    if (!isSupported())
        return;
    d->tabs = tabs;
    d->current = current;
    d->apply();
}

int TaskbarTabs::count() const
{
    return int(d->proxies.size());
}

#else

struct TaskbarTabs::Private
{};

TaskbarTabs::TaskbarTabs(QWidget* window)
    : QObject(window)
    , d(std::make_unique<Private>())
{}

TaskbarTabs::~TaskbarTabs() = default;

bool TaskbarTabs::isSupported()
{
    return false;
}

void TaskbarTabs::setTabs(const QList<Tab>&, QObject*) {}

int TaskbarTabs::count() const
{
    return 0;
}

#endif

} // namespace OFS

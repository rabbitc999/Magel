/*
 * gui_qt.cpp —— Qt Widgets 后端的简易 GUI 库（C ABI）。
 *
 * 布局与控件 ID 约定和 WinAPI 后端一致：固定像素布局，ID 1..65535 不重复。
 * 事件队列也是同一条：0 = 暂无事件，正数 = 被点击的按钮 ID，-1 = 窗口关闭。
 *
 * 与 WinAPI 后端的区别只有事件循环：Qt 版把事件处理拆成可重入的短时片
 * （`mcgui_pump` / `mcgui_wait_event`），这样 VM 能频繁拿回控制权、在指令边界
 * 交付中断；Qt 侧周期事件和 GUI 事件都通过 `mcgui_callback_t` 同步交回胶水层。
 *
 * 只要传定宽整数与 UTF-8 字符串，不把 QWidget 暴露给 MAGEL。
 */

#include "gui_qt.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtGui/QCloseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

#include <cstddef>
#include <cstdint>
#include <chrono>
#include <cstring>
#include <deque>
#include <map>
#include <string>

namespace
{

constexpr std::size_t kQueueCapacity = 256;
constexpr std::size_t kTextCapacity = 4096;
constexpr int kMaxTextLength = 1024;
constexpr std::size_t kWaitSlices = 50; /* 每次 wait 最多泵 50 片，之后交回 VM */

QApplication *g_app = nullptr;
QWidget *g_window = nullptr;
std::map<std::int32_t, QWidget *> g_controls;
std::deque<std::int32_t> g_events;
std::string g_text;
mcgui_callback_t g_callback = nullptr;
void *g_callbackUser = nullptr;
bool g_eventTimerActive = false;
std::int32_t g_eventTimerInterval = 0;
std::chrono::steady_clock::time_point g_eventTimerLast;
std::int64_t g_timerTicks = 0;

void notify(std::int32_t event, std::int64_t source);

/**
 * 真实窗口：关窗时入队 -1，和 WinAPI 的 WM_DESTROY 一致。
 */
class McWindow final : public QWidget
{
public:
    /**
     * @brief 接收窗口关闭事件并更新事件队列。
     * @param event 待处理的界面事件或事件编号。
     */
    void closeEvent(QCloseEvent *event) override
    {
        QWidget::closeEvent(event);
        notify(-1, 0);
    }
};

/**
 * @brief 将控件事件加入待处理队列。
 * @param event 待处理的界面事件或事件编号。
 */
void pushEvent(std::int32_t event)
{
    if (g_events.size() < kQueueCapacity) g_events.push_back(event);
}

/**
 * @brief 向语言客户端发送通知消息。
 *
 * 入队事件并就地通知回调（回调 → 胶水 → Machine::raiseInterrupt）。
 * @param event 待处理的界面事件或事件编号。
 * @param source 提供输入数据的来源。
 */
void notify(std::int32_t event, std::int64_t source)
{
    pushEvent(event);
    if (g_callback != nullptr) g_callback(static_cast<std::int64_t>(event), source, g_callbackUser);
}

/**
 * @brief 从待处理队列取出一个控件事件。
 * @retval 队列中的下一个界面事件编号。
 */
std::int32_t popEvent()
{
    if (g_events.empty()) return 0;
    const std::int32_t event = g_events.front();
    g_events.pop_front();
    return event;
}

/**
 * @brief 确保 Qt 应用实例已经初始化。
 * @retval 满足检查条件或操作完成时为 true，检查失败或无法完成时为 false。
 */
bool ensureApplication()
{
    if (g_app != nullptr) return true;
    static int argc = 1;
    static char program[] = "magel-gui-qt";
    static char *argv[] = { program, nullptr };
    g_app = new QApplication(argc, argv);
    return g_app != nullptr;
}

/**
 * @brief Qt 周期事件：到点就回调一次。
 *
 * 不用 QTimer：0ms 的 QTimer 会在一次 `processEvents` 里连着补跑一串，把中断队列打爆。
 * 这里改成由泵**显式**决定：interval==0 表示每个泵回合一次，>0 用单调时钟限速。
 */
void tickIfDue()
{
    if (!g_eventTimerActive || g_callback == nullptr) return;
    if (g_eventTimerInterval > 0)
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - g_eventTimerLast < std::chrono::milliseconds(g_eventTimerInterval)) return;
        g_eventTimerLast = now;
    }
    ++g_timerTicks;
    g_callback(0, g_timerTicks, g_callbackUser);
}

/**
 * @brief 只处理已经排队的事件，不等待：指令钩子每 N 条指令调一次，必须立刻返回。
 */
void pumpNow()
{
    if (g_app == nullptr) return;
    QCoreApplication::processEvents(QEventLoop::AllEvents, 0);
    tickIfDue();
}

/**
 * @brief 在窗口里加一个控件；`make` 负责建具体控件，`id` 重复会失败。
 * @param make 创建控件的工厂回调。
 * @param id 待查找或操作的对象编号。
 * @param x 控件横向坐标。
 * @param y 控件纵向坐标。
 * @param width 窗口或控件宽度。
 * @param height 窗口或控件高度。
 * @param text 待处理的文本。
 * @retval 在窗口里加一个控件；`make` 负责建具体控件，`id` 重复会失败。
 */
template <typename Factory>
int32_t addControl(Factory make, int32_t id, int32_t x, int32_t y, int32_t width,
                   int32_t height, const char *text)
{
    if (g_window == nullptr || id < 1 || id > 65535) return 0;
    if (g_controls.find(id) != g_controls.end()) return 0;
    if (x < 0 || y < 0 || width < 1 || height < 1) return 0;
    const QString content = QString::fromUtf8(text == nullptr ? "" : text);
    if (content.size() > kMaxTextLength) return 0;
    QWidget *control = make(content);
    if (control == nullptr) return 0;
    control->setParent(g_window);
    control->setGeometry(x, y, width, height);
    control->show();
    g_controls[id] = control;
    return 1;
}

}  // namespace

extern "C" {

/**
 * @brief 创建并显示图形窗口。
 * @param width 窗口或控件宽度。
 * @param height 窗口或控件高度。
 * @param title 窗口标题文本。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_open(int32_t width, int32_t height, const char *title)
{
    if (g_window != nullptr || width < 100 || height < 100 || width > 4096 || height > 4096) return 0;
    if (!ensureApplication()) return 0;
    const QString text = QString::fromUtf8(title == nullptr ? "" : title);
    if (text.size() > kMaxTextLength) return 0;
    McWindow *window = new McWindow();
    window->setWindowTitle(text);
    window->resize(width, height);
    g_window = window;
    g_events.clear();
    window->show();
    pumpNow();
    return 1;
}

/**
 * @brief 创建静态文本控件。
 * @param id 待查找或操作的对象编号。
 * @param x 控件横向坐标。
 * @param y 控件纵向坐标。
 * @param width 窗口或控件宽度。
 * @param height 窗口或控件高度。
 * @param text 待处理的文本。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_label(int32_t id, int32_t x, int32_t y, int32_t width,
                    int32_t height, const char *text)
{
    return addControl([&](const QString &content) -> QWidget * {
        QLabel *label = new QLabel(content, g_window);
        return label;
    },
                      id, x, y, width, height, text);
}

/**
 * @brief 创建按钮控件并接入点击事件。
 * @param id 待查找或操作的对象编号。
 * @param x 控件横向坐标。
 * @param y 控件纵向坐标。
 * @param width 窗口或控件宽度。
 * @param height 窗口或控件高度。
 * @param text 待处理的文本。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_button(int32_t id, int32_t x, int32_t y, int32_t width,
                     int32_t height, const char *text)
{
    return addControl([&](const QString &content) -> QWidget * {
        QPushButton *button = new QPushButton(content, g_window);
        QObject::connect(button, &QPushButton::clicked, button,
                         [id]() { notify(id, 0); });
        return button;
    },
                      id, x, y, width, height, text);
}

/**
 * @brief 创建文本输入控件。
 * @param id 待查找或操作的对象编号。
 * @param x 控件横向坐标。
 * @param y 控件纵向坐标。
 * @param width 窗口或控件宽度。
 * @param height 窗口或控件高度。
 * @param text 待处理的文本。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_text_box(int32_t id, int32_t x, int32_t y, int32_t width,
                       int32_t height, const char *text)
{
    return addControl([&](const QString &content) -> QWidget * {
        QLineEdit *edit = new QLineEdit(content, g_window);
        edit->setMaxLength(kMaxTextLength);
        return edit;
    },
                      id, x, y, width, height, text);
}

/**
 * @brief 按控件编号更新显示文本。
 * @param id 待查找或操作的对象编号。
 * @param text 待处理的文本。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_set_text(int32_t id, const char *text)
{
    if (g_window == nullptr) return 0;
    const auto found = g_controls.find(id);
    if (found == g_controls.end()) return 0;
    const QString content = QString::fromUtf8(text == nullptr ? "" : text);
    if (content.size() > kMaxTextLength) return 0;
    if (QLabel *label = qobject_cast<QLabel *>(found->second))
        label->setText(content);
    else if (QLineEdit *edit = qobject_cast<QLineEdit *>(found->second))
        edit->setText(content);
    else if (QPushButton *button = qobject_cast<QPushButton *>(found->second))
        button->setText(content);
    else
        return 0;
    return 1;
}

/**
 * @brief 按控件编号读取当前文本。
 * @param id 待查找或操作的对象编号。
 * @retval 控件当前文本的零结尾字符指针。
 */
const char *mcgui_get_text(int32_t id)
{
    g_text.clear();
    if (g_window == nullptr) return g_text.c_str();
    const auto found = g_controls.find(id);
    if (found == g_controls.end()) return g_text.c_str();
    QString content;
    if (QLabel *label = qobject_cast<QLabel *>(found->second))
        content = label->text();
    else if (QLineEdit *edit = qobject_cast<QLineEdit *>(found->second))
        content = edit->text();
    else if (QPushButton *button = qobject_cast<QPushButton *>(found->second))
        content = button->text();
    else
        return g_text.c_str();
    const QByteArray utf8 = content.toUtf8();
    if (utf8.size() > static_cast<int>(kTextCapacity - 1)) return g_text.c_str();
    g_text.assign(utf8.constData(), static_cast<std::size_t>(utf8.size()));
    return g_text.c_str();
}

/**
 * @brief 处理待办界面消息并尝试取出一个事件。
 * @retval 当前可处理的界面事件编号。
 */
int32_t mcgui_poll_event(void)
{
    if (g_app == nullptr) return -1;
    pumpNow();
    if (!g_events.empty()) return popEvent();
    return g_window == nullptr ? -1 : 0;
}

/**
 * @brief 等待并返回下一个界面事件。
 * @retval 等待取得的界面事件编号。
 */
int32_t mcgui_wait_event(void)
{
    if (g_window == nullptr) return -1;
    /* 分片等待：每片最多 2ms，期间照常派发 Qt 事件与回调。超时就返回 0，
     * 把控制权交回 VM，让它在指令边界交付中断（Qt 版 wait 不无限阻塞）。 */
    for (std::size_t slice = 0; slice < kWaitSlices; ++slice)
    {
        if (g_app == nullptr || g_window == nullptr) return -1;
        if (g_app != nullptr) QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
        tickIfDue();
        if (!g_events.empty()) return popEvent();
        if (g_window == nullptr) return -1;
    }
    return 0;
}

/**
 * @brief 关闭图形窗口并清理控件状态。
 */
void mcgui_close(void)
{
    if (g_window == nullptr) return;
    QWidget *window = g_window;
    g_window = nullptr;
    window->hide();
    window->deleteLater();
    pumpNow();
}

/**
 * @brief 推进 Qt 界面事件处理。
 */
void mcgui_pump(void) { pumpNow(); }

/**
 * @brief 向图形界面事件队列投递事件。
 * @param event 待处理的界面事件或事件编号。
 * @retval 控件编号或界面操作的状态码。
 */
int32_t mcgui_post_event(int32_t event)
{
    notify(event, 0);
    return 1;
}

/**
 * @brief 启动界面定时器并按间隔生成事件。
 * @param interval_ms 定时器触发间隔，单位为毫秒。
 */
void mcgui_start_timer(int32_t interval_ms)
{
    if (g_app == nullptr || interval_ms < 0) return;
    g_eventTimerInterval = interval_ms;
    g_eventTimerLast = std::chrono::steady_clock::now();
    g_timerTicks = 0;
    g_eventTimerActive = true;
}

/**
 * @brief 停止界面定时器。
 */
void mcgui_stop_timer(void) { g_eventTimerActive = false; }

/**
 * @brief 设置界面事件回调与用户上下文。
 * @param callback 接收事件或执行操作的回调函数。
 * @param user 传递给回调的用户上下文。
 * @retval 控件编号或界面操作的状态码。
 */
void mcgui_set_callback(mcgui_callback_t callback, void *user)
{
    g_callback = callback;
    g_callbackUser = user;
}

}  // extern "C"

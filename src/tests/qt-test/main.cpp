// NavigatorHMI RK3562 Qt 窗口测试程序 (B-1 循环)
// 验证: eglfs 渲染链 (屏幕显示) + 触摸/按键响应 + 中文字体
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QDebug>
#include <QElapsedTimer>
#include <QTouchEvent>

class TestWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TestWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowTitle("NavigatorHMI RK3562 Test");
        setStyleSheet("background-color: #1e3a5f;");

        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(16);

        auto *title = new QLabel("NavigatorHMI RK3562", this);
        title->setStyleSheet("color: white; font-size: 42px; font-weight: bold;");
        title->setAlignment(Qt::AlignCenter);

        auto *status = new QLabel("Qt 6.4.3 / eglfs 渲染正常", this);
        status->setObjectName("statusLabel");
        status->setStyleSheet("color: #7ec8ff; font-size: 26px;");
        status->setAlignment(Qt::AlignCenter);

        auto *hint = new QLabel("触摸屏幕任意位置改变颜色", this);
        hint->setObjectName("hintLabel");
        hint->setStyleSheet("color: #c0c0c0; font-size: 20px;");
        hint->setAlignment(Qt::AlignCenter);

        layout->addWidget(title);
        layout->addWidget(status);
        layout->addWidget(hint);
    }

protected:
    bool event(QEvent *e) override
    {
        // 触摸/鼠标事件统一响应: 改变背景色
        if (e->type() == QEvent::TouchBegin || e->type() == QEvent::TouchUpdate ||
            e->type() == QEvent::MouseButtonPress || e->type() == QEvent::KeyPress) {
            static const char *colors[] = {"#1e3a5f", "#3f1e5f", "#1e5f3a", "#5f1e1e", "#5f5a1e"};
            static int idx = 0;
            idx = (idx + 1) % 5;
            setStyleSheet(QString("background-color: %1;").arg(colors[idx]));
            qDebug() << "[qt-test] touch/key event -> color" << idx;
            return true;
        }
        return QWidget::event(e);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    qDebug() << "[qt-test] NavigatorHMI RK3562 Qt test starting...";

    TestWidget w;
    w.resize(1024, 600);   // 7 寸屏分辨率
    w.showFullScreen();     // eglfs 全屏

    // 心跳日志: 确认进程存活 (串口可观察)
    QElapsedTimer timer;
    timer.start();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        qDebug() << "[qt-test] quit";
    });

    return app.exec();
}

#include "main.moc"   // AUTOMOC: Q_OBJECT 类定义在 cpp 内, 需包含 moc 输出

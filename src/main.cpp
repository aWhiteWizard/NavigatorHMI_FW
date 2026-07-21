/*
 * @Author: aWhiteWizard www.123518341@qq.com
 * @FilePath: \NavigatorHMI_FW\src\main.cpp
 * @Description: NavigatorHMI 主程序 - Qt HMI 链路验证 Demo
 *               全屏窗口 + 中文标签 + 触摸按钮，用于验证:
 *               Qt5 交叉编译 / linuxfb 显示 / 中文字体 / evdev 触摸
 */
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("NavigatorHMI"));
    window.setStyleSheet(QStringLiteral("QWidget { background-color: #203864; }"));

    QVBoxLayout *layout = new QVBoxLayout(&window);

    QLabel *label = new QLabel(QStringLiteral("NavigatorHMI\n\nQt 5.12.9 @ i.MX6ULL"), &window);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("QLabel { color: white; font-size: 28px; }"));

    QPushButton *button = new QPushButton(QStringLiteral("触摸测试"), &window);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2E75B6; color: white; font-size: 24px; "
        "              border-radius: 8px; min-height: 64px; }"
        "QPushButton:pressed { background-color: #1F4E79; }"));

    QObject::connect(button, &QPushButton::clicked, label, [label]() {
        label->setText(QStringLiteral("触摸测试成功 ✓"));
    });

    layout->addWidget(label, 1);
    layout->addWidget(button);

    // 正点原子 7 寸屏 800x480（4.3 寸屏为 480x272，按实际修改）
    window.resize(800, 480);
    window.show();

    return app.exec();
}

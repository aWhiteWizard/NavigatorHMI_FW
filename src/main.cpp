/*
 * @Author: aWhiteWizard www.123518341@qq.com
 * @FilePath: \NavigatorHMI_FW\src\main.cpp
 * @Description: NavigatorHMI 主程序 - Qt HMI 链路验证 Demo
 *               全屏自适应 + 中文标签 + 触摸按钮 + RGB 色块
 *               用于验证: Qt5 交叉编译 / linuxfb 显示 / 中文字体 /
 *                        evdev 触摸 / RGB565 颜色通道是否正确
 */
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

// 创建一个纯色块标签（用于 RGB 通道诊断：显示颜色应与名称一致）
static QLabel* makeColorBlock(const QString &name, const QString &color, QWidget *parent)
{
    QLabel *label = new QLabel(name, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: white; font-size: 14px; }").arg(color));
    label->setMinimumHeight(48);
    return label;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle(QStringLiteral("NavigatorHMI"));
    window.setStyleSheet(QStringLiteral("QWidget { background-color: #203864; }"));

    QVBoxLayout *mainLayout = new QVBoxLayout(&window);

    QLabel *title = new QLabel(QStringLiteral("NavigatorHMI\nQt 5.12.9 @ i.MX6ULL"), &window);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet(QStringLiteral("QLabel { color: white; font-size: 20px; }"));

    // RGB 诊断色块：屏幕应显示 红/绿/蓝，若红蓝互换则需调整像素格式
    QHBoxLayout *colorLayout = new QHBoxLayout();
    colorLayout->addWidget(makeColorBlock(QStringLiteral("红 R"), QStringLiteral("#FF0000"), &window));
    colorLayout->addWidget(makeColorBlock(QStringLiteral("绿 G"), QStringLiteral("#00FF00"), &window));
    colorLayout->addWidget(makeColorBlock(QStringLiteral("蓝 B"), QStringLiteral("#0000FF"), &window));

    QPushButton *button = new QPushButton(QStringLiteral("触摸测试"), &window);
    button->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2E75B6; color: white; font-size: 18px; "
        "              border-radius: 8px; min-height: 48px; }"
        "QPushButton:pressed { background-color: #1F4E79; }"));

    QObject::connect(button, &QPushButton::clicked, title, [title]() {
        title->setText(QStringLiteral("触摸测试成功 ✓"));
    });

    mainLayout->addWidget(title, 1);
    mainLayout->addLayout(colorLayout);
    mainLayout->addWidget(button);

    // 全屏显示：自动适配帧缓冲实际分辨率（4.3寸 480x272 / 7寸 800x480 等）
    window.showFullScreen();

    return app.exec();
}

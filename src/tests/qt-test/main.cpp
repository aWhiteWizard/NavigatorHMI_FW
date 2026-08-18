// NavigatorHMI RK3562 测试界面 v2 (B-3)
// 功能: IP 显示 / 色块变色检视 / 内嵌终端交互(QProcess) / 退出按钮
// 运行: 默认 eglfs 屏幕显示; 远程调试: -platform vnc:port=5900
#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QProcess>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QDebug>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QVector>

// 色块控件: 定时变色 + 点击换色 + RGB 显示
class ColorBlock : public QWidget
{
    Q_OBJECT
public:
    explicit ColorBlock(QWidget *parent = nullptr) : QWidget(parent)
    {
        setAttribute(Qt::WA_StyledBackground);   // 让 stylesheet 背景对自定义 QWidget 生效
        setFixedSize(180, 100);
        setStyleSheet("border: 2px solid white; border-radius: 8px;");
        m_hue = QRandomGenerator::global()->bounded(360);
        updateColor();
    }

    void setHue(int h) { m_hue = h % 360; updateColor(); }

signals:
    void colorChanged(int r, int g, int b);

protected:
    void mousePressEvent(QMouseEvent *e) override
    {
        setHue(m_hue + 60);
        QWidget::mousePressEvent(e);
    }

private:
    void updateColor()
    {
        QColor c;
        c.setHsv(m_hue, 200, 230);
        m_r = c.red(); m_g = c.green(); m_b = c.blue();
        setStyleSheet(QString("background-color: rgb(%1,%2,%3); border: 2px solid white; border-radius: 8px;")
                          .arg(m_r).arg(m_g).arg(m_b));
        emit colorChanged(m_r, m_g, m_b);
    }
    int m_hue = 0, m_r = 0, m_g = 0, m_b = 0;
};

// 内嵌终端: QProcess bash 交互 (单命令执行)
class TermPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TermPanel(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_output = new QPlainTextEdit(this);
        m_output->setReadOnly(true);
        m_output->setMaximumBlockCount(500);
        m_output->setStyleSheet("background-color: black; color: #00ff00; font-family: monospace; font-size: 13px;");
        m_input = new QLineEdit(this);
        m_input->setPlaceholderText("输入命令 (回车执行, 如 ls / ifconfig / qt-test)");
        m_input->setStyleSheet("background-color: #111; color: white; font-family: monospace; font-size: 13px;");

        layout->addWidget(m_output, 1);
        layout->addWidget(m_input);

        connect(m_input, &QLineEdit::returnPressed, this, [this]() {
            QString cmd = m_input->text().trimmed();
            if (cmd.isEmpty()) return;
            m_output->appendPlainText("$ " + cmd);
            m_input->clear();
            auto *one = new QProcess(this);
            one->setProcessChannelMode(QProcess::MergedChannels);
            connect(one, &QProcess::readyReadStandardOutput, this, [this, one]() {
                m_output->appendPlainText(QString::fromUtf8(one->readAllStandardOutput()).trimmed());
            });
            connect(one, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    this, [one](int, QProcess::ExitStatus) { one->deleteLater(); });
            one->start("/bin/sh", QStringList() << "-c" << cmd);
        });
        m_output->appendPlainText("NavigatorHMI RK3562 终端 (Qt 6.4.3) - 输入命令测试");
    }

private:
    QPlainTextEdit *m_output;
    QLineEdit *m_input;
};

class TestWindow : public QWidget
{
    Q_OBJECT
public:
    explicit TestWindow(QWidget *parent = nullptr) : QWidget(parent)
    {
        setWindowTitle("NavigatorHMI RK3562 Test v2");
        setStyleSheet("background-color: #16233a;");

        auto *root = new QVBoxLayout(this);
        root->setSpacing(10);
        root->setContentsMargins(12, 10, 12, 10);

        // 标题 + IP
        auto *topRow = new QHBoxLayout();
        auto *title = new QLabel("NavigatorHMI RK3562", this);
        title->setStyleSheet("color: white; font-size: 26px; font-weight: bold;");
        m_ipLabel = new QLabel(this);
        m_ipLabel->setStyleSheet("color: #7ec8ff; font-size: 18px;");
        topRow->addWidget(title);
        topRow->addStretch();
        topRow->addWidget(m_ipLabel);
        root->addLayout(topRow);

        // 色块区 (体现 Qt 动画/信号槽/样式)
        auto *blockLabel = new QLabel("色块检视 (定时自动变色, 点击换色) - 体现 Qt 特性", this);
        blockLabel->setStyleSheet("color: #c0c0c0; font-size: 14px;");
        root->addWidget(blockLabel);

        auto *blocksRow = new QHBoxLayout();
        blocksRow->setSpacing(12);
        m_rgbLabel = new QLabel("RGB: -", this);
        m_rgbLabel->setStyleSheet("color: #ffd700; font-size: 15px;");
        for (int i = 0; i < 3; ++i) {
            auto *block = new ColorBlock(this);
            connect(block, &ColorBlock::colorChanged, this, [this](int r, int g, int b) {
                m_rgbLabel->setText(QString("RGB: (%1, %2, %3)").arg(r).arg(g).arg(b));
            });
            blocksRow->addWidget(block);
        }
        blocksRow->addStretch();
        root->addLayout(blocksRow);
        root->addWidget(m_rgbLabel);

        // 定时变色 (QTimer 动画)
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            static int tick = 0;
            QList<ColorBlock*> blocks = findChildren<ColorBlock*>();
            int i = 0;
            for (ColorBlock *b : blocks)
                b->setHue((i++ * 120 + tick * 7) % 360);
            tick++;
        });
        m_timer->start(400);

        // 终端
        auto *termLabel = new QLabel("内嵌终端 (QProcess 交互)", this);
        termLabel->setStyleSheet("color: #c0c0c0; font-size: 14px;");
        root->addWidget(termLabel);
        root->addWidget(new TermPanel(this), 1);

        // 退出按钮
        auto *btnRow = new QHBoxLayout();
        btnRow->addStretch();
        auto *quitBtn = new QPushButton("退出到系统 (quit)", this);
        quitBtn->setFixedSize(220, 44);
        quitBtn->setStyleSheet("QPushButton { background-color: #c0392b; color: white; font-size: 16px; border-radius: 6px; }"
                               "QPushButton:hover { background-color: #e74c3c; }");
        connect(quitBtn, &QPushButton::clicked, this, [this]() {
            qDebug() << "[qt-test] quit by user";
            qApp->quit();
        });
        btnRow->addWidget(quitBtn);
        root->addLayout(btnRow);

        updateIp();
        QTimer *ipTimer = new QTimer(this);
        connect(ipTimer, &QTimer::timeout, this, [this]() { updateIp(); });
        ipTimer->start(5000);
    }

private:
    void updateIp()
    {
        QString ip = "无网络";
        const auto addresses = QNetworkInterface::allAddresses();
        for (const QHostAddress &addr : addresses) {
            if (addr.protocol() == QAbstractSocket::IPv4Protocol &&
                addr.toString() != "127.0.0.1" &&
                !addr.toString().startsWith("169.254")) {
                ip = addr.toString();
                break;
            }
        }
        m_ipLabel->setText(QString("IP: %1").arg(ip));
    }

    QLabel *m_ipLabel;
    QLabel *m_rgbLabel;
    QTimer *m_timer;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qDebug() << "[qt-test v2] NavigatorHMI RK3562 starting (platform:"
             << qGuiApp->platformName() << ")";

    TestWindow w;
    w.resize(1024, 600);
    w.showFullScreen();

    return app.exec();
}

#include "main.moc"

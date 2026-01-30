#include <QStyle>
#include <QApplication>
#include <QMainWindow>
#include <QTabWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QSpinBox>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QCloseEvent>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QDialogButtonBox>

// 单个下载标签页的控件容器
class DownloadTab : public QWidget {
    Q_OBJECT
public:
    explicit DownloadTab(QWidget *parent = nullptr) : QWidget(parent) {
        // 初始化UI控件
        urlLabel = new QLabel(tr("下载链接:"), this);
        urlEdit = new QLineEdit(this);
        urlEdit->setPlaceholderText(tr("输入文件直链（如http/https/ftp）"));

        pathLabel = new QLabel(tr("保存位置:"), this);
        pathEdit = new QLineEdit(this);
        // 设置默认下载路径：/home/用户名/下载
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        pathEdit->setText(defaultPath);
        browseBtn = new QPushButton(tr("浏览"), this);

        threadLabel = new QLabel(tr("线程数:"), this);
        threadSpin = new QSpinBox(this);
        threadSpin->setRange(1, 16);
        threadSpin->setValue(16); // 默认16线程

        startBtn = new QPushButton(tr("开始下载"), this);
        stopBtn = new QPushButton(tr("停止下载"), this);
        stopBtn->setEnabled(false);

        // 布局设置
        QHBoxLayout *urlLayout = new QHBoxLayout;
        urlLayout->addWidget(urlLabel);
        urlLayout->addWidget(urlEdit);

        QHBoxLayout *pathLayout = new QHBoxLayout;
        pathLayout->addWidget(pathLabel);
        pathLayout->addWidget(pathEdit);
        pathLayout->addWidget(browseBtn);

        QHBoxLayout *threadLayout = new QHBoxLayout;
        threadLayout->addWidget(threadLabel);
        threadLayout->addWidget(threadSpin);
        threadLayout->addStretch();

        QHBoxLayout *btnLayout = new QHBoxLayout;
        btnLayout->addWidget(startBtn);
        btnLayout->addWidget(stopBtn);
        btnLayout->addStretch();

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->addLayout(urlLayout);
        mainLayout->addLayout(pathLayout);
        mainLayout->addLayout(threadLayout);
        mainLayout->addLayout(btnLayout);
        mainLayout->addStretch();

        // 绑定信号槽
        connect(browseBtn, &QPushButton::clicked, this, &DownloadTab::browsePath);
        connect(startBtn, &QPushButton::clicked, this, &DownloadTab::startDownload);
        connect(stopBtn, &QPushButton::clicked, this, &DownloadTab::stopDownload);

        // 初始化Aria2进程
        aria2Process = new QProcess(this);
        connect(aria2Process, &QProcess::finished, this, [=]() {
            startBtn->setEnabled(true);
            stopBtn->setEnabled(false);
        });
    }

    // 检查当前标签是否正在下载
    bool isDownloading() const {
        return aria2Process->state() == QProcess::Running;
    }

private slots:
    // 浏览保存路径
    void browsePath() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("选择保存目录"), pathEdit->text());
        if (!dir.isEmpty()) {
            pathEdit->setText(dir);
        }
    }

    // 开始下载
    void startDownload() {
        QString url = urlEdit->text().trimmed();
        QString path = pathEdit->text().trimmed();
        int threads = threadSpin->value();

        if (url.isEmpty()) {
            QMessageBox::warning(this, tr("错误"), tr("请输入有效的下载链接！"));
            return;
        }

        // 构建Aria2命令
        QStringList args;
        args << "--dir=" + path
             << "--split=" + QString::number(threads)
             << "--max-connection-per-server=" + QString::number(threads)
             << url;

        // 启动Aria2进程
        aria2Process->start("aria2c", args);
        if (aria2Process->waitForStarted()) {
            startBtn->setEnabled(false);
            stopBtn->setEnabled(true);
        } else {
            QMessageBox::critical(this, tr("错误"), tr("Aria2启动失败，请确保已安装aria2！"));
        }
    }

    // 停止下载
    void stopDownload() {
        if (aria2Process->state() == QProcess::Running) {
            aria2Process->kill();
            aria2Process->waitForFinished();
            startBtn->setEnabled(true);
            stopBtn->setEnabled(false);
        }
    }

private:
    QLabel *urlLabel;
    QLineEdit *urlEdit;
    QLabel *pathLabel;
    QLineEdit *pathEdit;
    QPushButton *browseBtn;
    QLabel *threadLabel;
    QSpinBox *threadSpin;
    QPushButton *startBtn;
    QPushButton *stopBtn;
    QProcess *aria2Process;
};

// 主窗口类
class ApritMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit ApritMainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle(tr("Aprit - 1.0.0"));
        setMinimumSize(600, 300);

        // 初始化设置（用于存储"以后不再提示"的选项）
        settings = new QSettings("Aprit", "ApritDownloader", this);

        // 菜单栏
        QMenuBar *menuBar = new QMenuBar(this);
        QMenu *fileMenu = new QMenu(tr("文件(&F)"), this);
        QAction *newTabAction = new QAction(tr("新建标签页(&N)"), this);
        QAction *exitAction = new QAction(tr("退出(&E)"), this);
        fileMenu->addAction(newTabAction);
        fileMenu->addAction(exitAction);

        QMenu *helpMenu = new QMenu(tr("帮助(&H)"), this);
        QAction *aboutAction = new QAction(tr("关于(&A)"), this);
        helpMenu->addAction(aboutAction);

        menuBar->addMenu(fileMenu);
        menuBar->addMenu(helpMenu);
        setMenuBar(menuBar);

        // 多标签页控件（最多4个）
        tabWidget = new QTabWidget(this);
        tabWidget->setTabsClosable(true);
        tabWidget->setMovable(true);
        setCentralWidget(tabWidget);

        // 添加第一个默认标签页
        addNewTab();

        // 绑定信号槽
        connect(newTabAction, &QAction::triggered, this, &ApritMainWindow::addNewTab);
        connect(exitAction, &QAction::triggered, this, &ApritMainWindow::close);
        connect(aboutAction, &QAction::triggered, this, &ApritMainWindow::showAbout);
        connect(tabWidget, &QTabWidget::tabCloseRequested, this, [=](int index) {
            // 关闭标签页时检查是否正在下载
            DownloadTab *tab = qobject_cast<DownloadTab*>(tabWidget->widget(index));
            if (tab && tab->isDownloading()) {
                QMessageBox::warning(this, tr("提示"), tr("该标签页正在下载，无法关闭！"));
                return;
            }
            tabWidget->removeTab(index);
        });
    }

protected:
    // 重写关闭事件
    void closeEvent(QCloseEvent *event) override {
        // 检查是否需要提示
        bool needPrompt = settings->value("showClosePrompt", true).toBool();
        if (!needPrompt) {
            event->accept();
            return;
        }

        // 检查是否有下载中的标签或多个标签页
        bool hasDownloading = false;
        for (int i = 0; i < tabWidget->count(); ++i) {
            DownloadTab *tab = qobject_cast<DownloadTab*>(tabWidget->widget(i));
            if (tab && tab->isDownloading()) {
                hasDownloading = true;
                break;
            }
        }
        bool hasMultiTabs = tabWidget->count() > 1;

        if (!hasDownloading && !hasMultiTabs) {
            event->accept();
            return;
        }

        // 显示关闭提示对话框
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("提示"));
        msgBox.setText(tr("当前有下载任务进行中或多个标签页未关闭，确定要退出吗？"));
        QPushButton *noPromptBtn = msgBox.addButton(tr("以后不再提示"), QMessageBox::ActionRole);
        QPushButton *closeBtn = msgBox.addButton(tr("关闭"), QMessageBox::DestructiveRole);
        QPushButton *cancelBtn = msgBox.addButton(tr("取消"), QMessageBox::RejectRole);
        msgBox.exec();

        if (msgBox.clickedButton() == noPromptBtn) {
            settings->setValue("showClosePrompt", false);
            event->accept();
        } else if (msgBox.clickedButton() == closeBtn) {
            // 停止所有下载进程
            for (int i = 0; i < tabWidget->count(); ++i) {
                DownloadTab *tab = qobject_cast<DownloadTab*>(tabWidget->widget(i));
                if (tab && tab->isDownloading()) {
                    // 调用停止下载（需通过元对象，因为是private槽）
                    QMetaObject::invokeMethod(tab, "stopDownload");
                }
            }
            event->accept();
        } else {
            event->ignore();
        }
    }

private slots:
    // 添加新标签页（最多4个）
    void addNewTab() {
        if (tabWidget->count() >= 4) {
            QMessageBox::information(this, tr("提示"), tr("最多只能打开4个标签页！"));
            return;
        }
        DownloadTab *newTab = new DownloadTab(this);
        int tabIndex = tabWidget->addTab(newTab, tr("下载 %1").arg(tabWidget->count() + 1));
        tabWidget->setCurrentIndex(tabIndex);
    }

    // 显示关于对话框
    void showAbout() {
        QMessageBox::about(this, tr("关于 Aprit"), 
            tr("Aprit 1.0.0\n基于 Aria2 的轻量级 Linux 下载工具\n使用 Qt6 开发"));
    }

private:
    QTabWidget *tabWidget;
    QSettings *settings;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // 启用高DPI支持
    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
    app.setAttribute(Qt::AA_EnableHighDpiScaling);
    // 跟随系统主题（Qt6默认支持）
    app.setStyle(QApplication::style()->objectName());

    ApritMainWindow window;
    window.show();

    return app.exec();
}

#include "aprit_1.0.0.moc" // Qt6 moc编译需要

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp> // 引入 DNN 模块
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QDir>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , cap(0) // Initialize VideoCapture last
    , timer(new QTimer(this))
    , videoLabel(new QLabel(this))
    , detectLabel(new QLabel(this))
    , employeeIDEdit(new QLineEdit(this))
    , employeeNameEdit(new QLineEdit(this))
    , departmentEdit(new QLineEdit(this))
    , captureButton(new QPushButton("录入", this))
    , startDetectButton(new QPushButton("开始检测", this))
    , stopDetectButton(new QPushButton("停止检测", this))
    , isRecording(false)
    , isOnRecordPage(true)
    , isDetecting(false) // Initialize detection flag
    , detecting(false)
{
    ui->setupUi(this);
    initializeDatabase();
    initializeDirectories();
    setupUI();
    setWindowTitle("人事考勤系统"); // 设置窗口标题
    initializeDNN(); // 初始化 DNN 模型

    QString faceCascadePath = "D:/code/qt_project/OpenCV_Face/haarcascade_frontalface_default.xml";
    if (!faceCascade.load(faceCascadePath.toStdString())) {
        qDebug() << "Error loading face cascade from: " << faceCascadePath;
        return;
    }

    if (!cap.isOpened()) {
        qDebug() << "Error opening video stream or file";
        return;
    }

    connect(timer, &QTimer::timeout, this, &MainWindow::processFrameAndUpdateGUI);
    connect(captureButton, &QPushButton::clicked, this, &MainWindow::captureFaceAndRecord);
    connect(startDetectButton, &QPushButton::clicked, this, &MainWindow::startDetection);
    connect(stopDetectButton, &QPushButton::clicked, this, &MainWindow::stopDetection);
    connect(tabWidget, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    timer->start(30);

    // 设置默认的标签页为检测页面
    tabWidget->setCurrentIndex(1);


}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::initializeDirectories() {
    // 创建存储人脸图像的目录，如果目录不存在的话
    QDir dir("D:/code/qt_project/OpenCV_Face/faces");
    if (!dir.exists()) {
        dir.mkpath("."); // 创建目录及其所有父目录
    }
}

void MainWindow::initializeDatabase() {
    // 初始化SQLite数据库
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("D:/code/qt_project/OpenCV_Face/attendance.db");

    // 打开数据库连接
    if (!db.open()) {
        QMessageBox::critical(this, "数据库错误", "无法打开数据库"); // 如果无法打开数据库，弹出错误对话框
        return;
    }

    QSqlQuery query;
    // 创建员工表，如果表不存在的话
    if (!query.exec("CREATE TABLE IF NOT EXISTS employees ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, " // 自动递增的主键
                    "employee_id TEXT UNIQUE, " // 员工编号，唯一
                    "name TEXT, " // 姓名
                    "department TEXT, " // 部门
                    "face_image TEXT)")) { // 人脸图像的文件名或路径
        qDebug() << "Error creating employees table:" << query.lastError().text(); // 如果创建表失败，输出错误信息
    }

    // 创建考勤记录表，如果表不存在的话
    if (!query.exec("CREATE TABLE IF NOT EXISTS attendance ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, " // 自动递增的主键
                    "employee_id TEXT, " // 员工编号
                    "name TEXT, " // 姓名
                    "department TEXT, " // 部门
                    "timestamp TEXT, " // 签到时间
                    "face_image TEXT)")) { // 人脸图像的文件名或路径
        qDebug() << "Error creating attendance table:" << query.lastError().text(); // 如果创建表失败，输出错误信息
    }
}


void MainWindow::setupUI() {
    tabWidget = new QTabWidget(this); // 创建一个选项卡小部件

    // 创建一个中央部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 设置主窗口的背景图片
    QString styleSheet = "QMainWindow { background-image: url(D:/code/qt_project/OpenCV_Face/6.jpg); background-repeat: no-repeat; background-position: center; }";
    this->setStyleSheet(styleSheet);

    // 设置录入页面
    QWidget *recordPage = new QWidget(this);
    QHBoxLayout *recordLayout = new QHBoxLayout;
    QVBoxLayout *leftRecordLayout = new QVBoxLayout;
    QFormLayout *formLayout = new QFormLayout;

    // 创建视频显示标签
    videoLabel = new QLabel(this);
    videoLabel->setFixedSize(640, 480);
    videoLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 5px;"); // 添加边框和圆角
    leftRecordLayout->addWidget(videoLabel);

    // 创建表单输入字段和按钮
    departmentEdit = new QLineEdit(this);
    employeeIDEdit = new QLineEdit(this);
    employeeNameEdit = new QLineEdit(this);
    captureButton = new QPushButton("录入", this);

    QLabel *departmentLabel = new QLabel("部门:", this);
    QLabel *employeeIDLabel = new QLabel("员工编号:", this);
    QLabel *employeeNameLabel = new QLabel("姓名:", this);

    QFont font;
    font.setPointSize(12);
    departmentLabel->setFont(font);
    employeeIDLabel->setFont(font);
    employeeNameLabel->setFont(font);
    departmentEdit->setFont(font);
    employeeIDEdit->setFont(font);
    employeeNameEdit->setFont(font);
    captureButton->setFont(font);

    captureButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 5px; border-radius: 5px;");

    // 使用 QFormLayout 布局输入字段
    formLayout->addRow(departmentLabel, departmentEdit);
    formLayout->addRow(employeeIDLabel, employeeIDEdit);
    formLayout->addRow(employeeNameLabel, employeeNameEdit);
    formLayout->addRow(captureButton);

    formLayout->setLabelAlignment(Qt::AlignRight); // 标签右对齐
    formLayout->setSpacing(15); // 增加行间距
    formLayout->setContentsMargins(15, 10, 15, 10); // 设置表单的边距

    // 设置 QLineEdit 的样式
    departmentEdit->setStyleSheet("background-color: #ffffff; border: 1px solid #cccccc; border-radius: 5px; padding: 5px;");
    employeeIDEdit->setStyleSheet("background-color: #ffffff; border: 1px solid #cccccc; border-radius: 5px; padding: 5px;");
    employeeNameEdit->setStyleSheet("background-color: #ffffff; border: 1px solid #cccccc; border-radius: 5px; padding: 5px;");

    QWidget *rightRecordWidget = new QWidget(this);
    QVBoxLayout *rightRecordLayout = new QVBoxLayout;
    rightRecordLayout->addLayout(formLayout);
    rightRecordWidget->setLayout(rightRecordLayout);

    rightRecordWidget->setStyleSheet("background-color: rgba(255, 255, 255, 0.8); padding: 10px; border-radius: 5px;"); // 添加背景色和内边距

    // 使用 spacer 使表单在垂直方向上居中
    QSpacerItem *spacerTop = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QSpacerItem *spacerBottom = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    leftRecordLayout->addSpacerItem(spacerTop);
    leftRecordLayout->addWidget(rightRecordWidget);
    leftRecordLayout->addSpacerItem(spacerBottom);

    recordLayout->addLayout(leftRecordLayout);
    recordPage->setLayout(recordLayout);

    // 设置检测页面
    QWidget *detectPage = new QWidget(this);
    QVBoxLayout *detectLayout = new QVBoxLayout;

    // 创建视频显示容器
    QWidget *videoContainer = new QWidget(this);
    QVBoxLayout *videoLayout = new QVBoxLayout;
    detectLabel = new QLabel(this);
    detectLabel->setFixedSize(640, 480);
    detectLabel->setStyleSheet("border: 2px solid #ddd; border-radius: 5px;"); // 添加边框和圆角
    videoLayout->addWidget(detectLabel);
    videoLayout->setAlignment(Qt::AlignCenter); // 使 detectLabel 居中
    videoContainer->setLayout(videoLayout);

    // 添加开始和停止按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout;
    startDetectButton = new QPushButton("开始检测", this);
    stopDetectButton = new QPushButton("停止检测", this);

    startDetectButton->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 5px; border-radius: 5px;");
    stopDetectButton->setStyleSheet("background-color: #f44336; color: white; font-weight: bold; padding: 5px; border-radius: 5px;");

    startDetectButton->setFont(font);
    stopDetectButton->setFont(font);

    buttonLayout->addWidget(startDetectButton);
    buttonLayout->addWidget(stopDetectButton);
    buttonLayout->setAlignment(Qt::AlignCenter); // 使按钮居中

    detectLayout->addWidget(videoContainer);
    detectLayout->addLayout(buttonLayout);
    detectPage->setLayout(detectLayout);

    // 设置记录表格页面
    QWidget *recordPage2 = new QWidget(this);
    QVBoxLayout *recordLayout2 = new QVBoxLayout;
    QTableWidget *recordTable = new QTableWidget(this);
    recordTable->setObjectName("recordTable");
    recordTable->setColumnCount(5);
    recordTable->setHorizontalHeaderLabels({"员工编号", "姓名", "部门", "签到时间", "人脸图像"});

    recordTable->setStyleSheet("QTableWidget { border: 1px solid #ddd; padding: 5px; }"
                               "QHeaderView::section { background-color: #f4f4f4; font-weight: bold; }");

    recordLayout2->addWidget(recordTable);

    QPushButton *refreshButton = new QPushButton("刷新", this);
    refreshButton->setFont(font);
    refreshButton->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold; padding: 5px; border-radius: 5px;");

    recordLayout2->addWidget(refreshButton);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::updateRecordTable);

    recordPage2->setLayout(recordLayout2);

    tabWidget->addTab(recordPage, "人脸录入");
    tabWidget->addTab(detectPage, "考勤签到");
    tabWidget->addTab(recordPage2, "考勤记录");

    setCentralWidget(tabWidget);

    // 设置状态栏
    QStatusBar *statusBar = new QStatusBar(this);
    QLabel *copyrightLabel = new QLabel("版权所有 © 13", this);
    statusBar->addWidget(copyrightLabel);
    setStatusBar(statusBar);
}



void MainWindow::initializeDNN() {
    // 模型文件路径
    std::string modelFile = "D:/code/qt_project/OpenCV_Face/models/deploy.prototxt";
    // 权重文件路径
    std::string weightFile = "D:/code/qt_project/OpenCV_Face/models/res10_300x300_ssd_iter_140000.caffemodel";

    // 从 Caffe 模型和权重文件中读取深度神经网络（DNN）
    net = cv::dnn::readNetFromCaffe(modelFile, weightFile);

    // 检查网络是否成功加载
    if (net.empty()) {
        // 如果网络为空，则输出错误信息
        qDebug() << "Error loading DNN model.";
    }
}


void MainWindow::processFrameAndUpdateGUI() {
    // 创建一个 OpenCV 的 Mat 对象来存储从摄像头捕获的图像
    cv::Mat frame;
    cap >> frame; // 从摄像头捕获一帧图像

    // 如果帧为空，直接返回，不进行后续处理
    if (frame.empty()) {
        return;
    }

    // 如果正在进行检测，才进行人脸检测
    if (detecting) {
        cv::Mat gray;
        // 将帧从 BGR 格式转换为灰度图像
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

        std::vector<cv::Rect> faces;
        // 使用人脸检测级联分类器检测图像中的人脸
        faceCascade.detectMultiScale(gray, faces);

        // 遍历检测到的人脸区域
        for (const auto &face : faces) {
            // 在帧中绘制人脸矩形框
            cv::rectangle(frame, face, cv::Scalar(255, 0, 0), 2);

            // 提取人脸区域
            cv::Mat faceROI = frame(face);

            // 如果人脸区域是灰度图像，转换为 BGR 格式以便进行后续处理
            if (faceROI.channels() == 1) {
                cv::cvtColor(faceROI, faceROI, cv::COLOR_GRAY2BGR);
            }

            // 进行面部检测和记录考勤
            detectAndRecordAttendance(faceROI);
        }
    }

    // 将 BGR 格式的图像转换为 RGB 格式以便于 Qt 使用
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
    // 将 OpenCV 的 Mat 图像转换为 QImage
    QImage img = QImage((const unsigned char*)(frame.data), frame.cols, frame.rows, frame.step, QImage::Format_RGB888);

    // 更新录入页面的视频标签显示图像
    videoLabel->setPixmap(QPixmap::fromImage(img));

    // 更新检测页面的视频标签显示图像
    detectLabel->setPixmap(QPixmap::fromImage(img));
}





void MainWindow::captureFaceAndRecord() {
    // 判断当前是否在录入模式
    if (!isRecording) {
        // 如果不在录入模式，设置为录入模式并更新按钮文本
        isRecording = true;
        captureButton->setText("停止");
    } else {
        // 如果已经在录入模式，停止录入并更新按钮文本
        isRecording = false;
        captureButton->setText("录入");

        // 捕捉当前帧图像
        cv::Mat frame;
        cap >> frame; // 从摄像头捕获一帧图像

        // 检查图像是否为空
        if (frame.empty()) {
            // 如果图像为空，弹出警告框
            QMessageBox::warning(this, "捕捉错误", "无法捕捉到面部图像");
            return;
        }

        // 将图像转换为灰度图像以进行面部检测
        cv::Mat gray;
        cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        std::vector<cv::Rect> faces;
        // 使用人脸检测级联分类器检测图像中的人脸
        faceCascade.detectMultiScale(gray, faces);

        // 检查是否检测到人脸
        if (faces.empty()) {
            // 如果没有检测到人脸，弹出警告框
            QMessageBox::warning(this, "面部检测错误", "未检测到面部");
            return;
        }

        // 提取检测到的第一个人脸区域
        cv::Mat faceROI = frame(faces[0]);

        // 将 BGR 格式的图像转换为 RGB 格式
        cv::Mat faceRGB;
        cv::cvtColor(faceROI, faceRGB, cv::COLOR_BGR2RGB);

        // 生成保存图像的文件名
        QString fileName = QString("D:/code/qt_project/OpenCV_Face/faces/%1.jpg").arg(employeeIDEdit->text());
        // 将提取的人脸图像保存为文件
        cv::imwrite(fileName.toStdString(), faceRGB);

        // 插入员工信息到数据库
        QSqlQuery query;
        query.prepare("INSERT INTO employees (employee_id, name, department, face_image) VALUES (?, ?, ?, ?)");
        query.addBindValue(employeeIDEdit->text()); // 员工编号
        query.addBindValue(employeeNameEdit->text()); // 姓名
        query.addBindValue(departmentEdit->text()); // 部门
        query.addBindValue(fileName); // 人脸图像文件名
        if (!query.exec()) {
            // 如果数据库操作失败，弹出警告框
            QMessageBox::warning(this, "录入错误", "无法录入员工信息");
        } else {
            // 如果数据库操作成功，弹出信息框
            QMessageBox::information(this, "录入成功", "员工信息已成功录入");
        }
    }
}


void MainWindow::showAttendanceRecords() {
    // 查询考勤记录
    QSqlQuery query("SELECT employee_id, name, department, timestamp, face_image FROM attendance");

    // 执行查询
    if (!query.exec()) {
        // 如果查询执行失败，输出错误信息
        qDebug() << "Error fetching attendance records:" << query.lastError().text();
        return;
    }

    // 用于存储所有考勤记录的字符串
    QString records;
    // 遍历查询结果
    while (query.next()) {
        // 获取每条记录的各个字段
        QString employeeID = query.value(0).toString(); // 员工编号
        QString name = query.value(1).toString(); // 姓名
        QString department = query.value(2).toString(); // 部门
        QString timestamp = query.value(3).toString(); // 签到时间
        QString faceImagePath = query.value(4).toString(); // 人脸图像的完整路径

        // 从完整路径中提取文件名
        QString faceImageName = QFileInfo(faceImagePath).fileName();

        // 将记录格式化为字符串并添加到记录列表中
        records += QString("部门: %1\n员工编号: %2\n姓名: %3\n签到时间: %4\n人脸图像: %5\n\n")
                       .arg(department).arg(employeeID).arg(name).arg(timestamp).arg(faceImageName);
    }

    // 弹出信息框显示所有考勤记录
    QMessageBox::information(this, "Attendance Records", records);
}

void MainWindow::detectAndRecordAttendance(const cv::Mat &faceROI) {
    static QDateTime lastAttendanceTime; // 记录上次签到时间
    double maxSimilarity = 0.5; // 设置一个适当的相似度阈值
    QString matchedEmployeeID = "unknown"; // 匹配到的员工编号
    QString matchedEmployeeName = "unknown"; // 匹配到的员工姓名
    QString matchedDepartment = "unknown"; // 匹配到的员工部门
    QString matchedFaceImage; // 匹配到的员工人脸图像路径

    // 遍历数据库中所有员工的人脸图像
    QSqlQuery query("SELECT employee_id, name, department, face_image FROM employees");
    while (query.next()) {
        QString employeeID = query.value(0).toString(); // 员工编号
        QString employeeName = query.value(1).toString(); // 员工姓名
        QString department = query.value(2).toString(); // 员工部门
        QString faceImagePath = query.value(3).toString(); // 员工人脸图像路径

        // 从文件中加载保存的人脸图像
        cv::Mat savedFace = cv::imread(faceImagePath.toStdString());
        if (savedFace.empty()) {
            continue; // 如果无法加载图像，则跳过
        }

        // 比较当前人脸图像与保存的人脸图像
        double similarity = calculateSimilarity(faceROI, savedFace);
        if (similarity < maxSimilarity) { // 如果相似度小于阈值，则更新匹配记录
            maxSimilarity = similarity;
            matchedEmployeeID = employeeID;
            matchedEmployeeName = employeeName;
            matchedDepartment = department;
            matchedFaceImage = faceImagePath;
        }
    }

    // 检查匹配的员工是否在最近已经签到
    if (maxSimilarity < 0.5) { // 如果相似度小于阈值，则记录签到信息
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

        // 防止频繁重复签到
        if (lastAttendanceTime.isValid() && lastAttendanceTime.secsTo(QDateTime::currentDateTime()) < 60) {
            return; // 如果上次签到时间距离现在小于60秒，则忽略
        }

        // 将签到记录插入到数据库中
        QSqlQuery query;
        query.prepare("INSERT INTO attendance (employee_id, name, department, timestamp, face_image) VALUES (?, ?, ?, ?, ?)");
        query.addBindValue(matchedEmployeeID);
        query.addBindValue(matchedEmployeeName);
        query.addBindValue(matchedDepartment);
        query.addBindValue(timestamp);
        query.addBindValue(matchedFaceImage);
        if (!query.exec()) {
            qDebug() << "Error recording attendance:" << query.lastError().text(); // 记录签到信息出错
        } else {
            lastAttendanceTime = QDateTime::currentDateTime(); // 更新上次签到时间
            QMessageBox::information(this, "签到成功", QString("部门: %1\n员工编号: %2\n姓名: %3\n签到时间: %4\n人脸图像: %5")
                                     .arg(matchedDepartment).arg(matchedEmployeeID).arg(matchedEmployeeName).arg(timestamp).arg(matchedFaceImage));
        }
    } else {
        QMessageBox::warning(this, "识别失败", "未能识别员工面部"); // 如果识别失败，则显示警告信息
    }
}




void MainWindow::onTabChanged(int index) {
    // 检查当前选中的标签页索引
    if (index == 1) { // 如果选中的是“考勤签到”页面
        isOnRecordPage = false; // 设置标志位，表示当前不在“录入”页面
    } else if (index == 0) { // 如果选中的是“人脸录入”页面
        isOnRecordPage = true; // 设置标志位，表示当前在“录入”页面
    }
}


double MainWindow::calculateSimilarity(const cv::Mat &face1, const cv::Mat &face2) {
    cv::Mat resizedFace1, resizedFace2;

    // 确保两个图像大小和通道数相同
    if (face1.size() != face2.size() || face1.channels() != face2.channels()) {
        // 如果图像大小或通道数不同，调整 face2 的大小以匹配 face1
        cv::resize(face2, resizedFace2, face1.size());
        resizedFace1 = face1;
    } else {
        resizedFace1 = face1;
        resizedFace2 = face2;
    }

    // 如果图像是彩色图像，则转换为灰度图像
    if (resizedFace1.channels() == 3) {
        cv::cvtColor(resizedFace1, resizedFace1, cv::COLOR_BGR2GRAY);
        cv::cvtColor(resizedFace2, resizedFace2, cv::COLOR_BGR2GRAY);
    }

    // 计算直方图
    cv::Mat hist1, hist2;
    int histSize[1] = {256}; // 直方图的大小（256个灰度级）
    float range[] = {0, 256}; // 直方图的范围
    const float* histRange[] = {range};

    // 计算第一个图像的直方图
    cv::calcHist(&resizedFace1, 1, 0, cv::Mat(), hist1, 1, histSize, histRange);
    cv::normalize(hist1, hist1, 1, 0, cv::NORM_L1); // 归一化直方图

    // 计算第二个图像的直方图
    cv::calcHist(&resizedFace2, 1, 0, cv::Mat(), hist2, 1, histSize, histRange);
    cv::normalize(hist2, hist2, 1, 0, cv::NORM_L1); // 归一化直方图

    // 使用巴氏距离（Bhattacharyya distance）比较两个直方图
    return cv::compareHist(hist1, hist2, cv::HISTCMP_BHATTACHARYYA);
}

void MainWindow::startDetection() {
    detecting = true; // 设置检测标志为真，表示开始检测
    startDetectButton->setEnabled(false); // 禁用“开始检测”按钮
    stopDetectButton->setEnabled(true); // 启用“停止检测”按钮
}

void MainWindow::stopDetection() {
    detecting = false; // 设置检测标志为假，表示停止检测
    startDetectButton->setEnabled(true); // 启用“开始检测”按钮
    stopDetectButton->setEnabled(false); // 禁用“停止检测”按钮
}

void MainWindow::updateRecordTable() {
    // 查找表格控件
    QTableWidget *recordTable = findChild<QTableWidget *>("recordTable");
    if (!recordTable) {
        qDebug() << "Error: recordTable not found!"; // 如果未找到表格控件，则输出错误信息
        return;
    }

    // 查询考勤记录
    QSqlQuery query("SELECT employee_id, name, department, timestamp, face_image FROM attendance");

    if (!query.exec()) {
        qDebug() << "Error fetching attendance records:" << query.lastError().text(); // 如果查询失败，则输出错误信息
        return;
    }

    // 清空表格
    recordTable->setRowCount(0);

    int row = 0;
    // 遍历查询结果
    while (query.next()) {
        recordTable->insertRow(row); // 插入新行
        recordTable->setItem(row, 0, new QTableWidgetItem(query.value(0).toString())); // 设置员工编号
        recordTable->setItem(row, 1, new QTableWidgetItem(query.value(1).toString())); // 设置姓名
        recordTable->setItem(row, 2, new QTableWidgetItem(query.value(2).toString())); // 设置部门
        recordTable->setItem(row, 3, new QTableWidgetItem(query.value(3).toString())); // 设置签到时间
        recordTable->setItem(row, 4, new QTableWidgetItem(query.value(4).toString())); // 设置人脸图像的名称或路径
        row++;
    }
}



#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTabWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <QTableWidget>
#include <QFormLayout>
#include <QWidget>
#include <QVBoxLayout>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void processFrameAndUpdateGUI();//处理摄像头帧并更新GUI
    void captureFaceAndRecord();// 捕捉人脸并记录员工信息
    void showAttendanceRecords();//显示考勤记录
    void startDetection();//开始人脸检测
    void stopDetection();//停止人脸检测
    void onTabChanged(int index);//当选项卡切换时的处理函数 index为当前选项卡的索引
    void updateRecordTable();

private:
    void initializeDatabase();//初始化SQLite数据库
    void initializeDirectories();//初始化所需目录
    void setupUI();// 设置用户界面
    void initializeDNN();//初始化深度神经网络(DNN)模型
    void detectAndRecordAttendance(const cv::Mat &faceROI);//检测并记录考勤信息   faceROI为传入的人脸区域图像
    double calculateSimilarity(const cv::Mat &face1, const cv::Mat &face2);//计算两张人脸图像的相似度 face1，2为第一，二张人脸图像 返回相似度分数

    Ui::MainWindow *ui;
    cv::VideoCapture cap;//视频捕获对象，用于从摄像头获取视频帧
    cv::CascadeClassifier faceCascade;//人脸检测分类器对象，用于检测图像中的人脸
    cv::dnn::Net net;//深度神经网络对象，用于人脸识别
    QSqlDatabase db;//SQLite数据库对象，用于存储员工信息和考勤记录
    QTimer *timer;//定时器对象，用于定时获取视频帧
    QLabel *videoLabel;//QLabel对象，用于显示视频流
    QLabel *detectLabel;//QLabel对象，用于显示检测状态信息
    QLabel *detectIDLabel;//QLabel对象，用于显示检测到的员工编号
    QLabel *detectNameLabel;//QLabel对象，用于显示检测到的员工姓名
    QLabel *detectDepartmentLabel;//QLabel对象，用于显示检测到的员工部门
    QLabel *detectTimeLabel;//QLabel对象，用于显示检测到的时间
    QLineEdit *employeeIDEdit;//QLineEdit对象，用于输入员工编号
    QLineEdit *employeeNameEdit;//QLineEdit对象，用于输入员工姓名
    QLineEdit *departmentEdit;//QLineEdit对象，用于输入员工部门
    QPushButton *captureButton;//QPushButton对象，用于触发捕捉人脸并记录员工信息的功能
    QPushButton *startDetectButton;//QPushButton对象，用于触发开始人脸检测功能
    QPushButton *stopDetectButton;// QPushButton对象，用于触发停止人脸检测功能
    QTabWidget *tabWidget;//QTabWidget对象，用于管理不同功能的选项卡界面
    QTableWidget *recordTable;//QTableWidget对象，用于显示考勤记录表
    QWidget *videoContainer;//QWidget对象，用于容纳视频显示组件
    QVBoxLayout *buttonLayout;//QVBoxLayout对象，用于布局按钮
    bool isRecording;//标志变量，指示是否正在录入员工信息
    bool isOnRecordPage;// 标志变量，指示当前是否在录入页面
    bool isDetecting;//标志变量，指示当前是否正在进行人脸检测
    bool detecting;//标志变量，指示是否正在进行检测
};

#endif // MAINWINDOW_H

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// Define this BEFORE including any Qt headers to avoid conflicts with libcamera
#define QT_NO_KEYWORDS

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QGroupBox>
#include <QRadioButton>
#include <QComboBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QScrollArea>
#include <QFrame>
#include <QSplitter>
#include <opencv2/opencv.hpp>
#include <memory>
#include <atomic>

// Forward declarations
class Application;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Public method to update video display from Application
    void updateVideoFrame(const cv::Mat& frame);
    void updateVideoFrameThreadSafe(const cv::Mat& frame);
    void updateTrackingFrameThreadSafe(const cv::Mat& frame);

Q_SIGNALS:
    void startRequested();
    void stopRequested();
    void pauseRequested();
    void configurationChanged();
    void videoFrameReady(const QImage& image);
    void trackingFrameReady(const QImage& image);

public Q_SLOTS:
    void onVideoFrameReady(const QImage& image);
    void onTrackingFrameReady(const QImage& image);
    void updateStatus(const QString& status);
    void updateFPS(double fps);
    void updateDetectionStats(int detectionCount);
    void updateTrackingInfo(bool tracking, const QString& className, float confidence);
    void updateMAVLinkStatus(bool connected, uint32_t flightMode, bool armed, float altitude, float battery);
    void updateResourceStats(float cpuUsage, float memoryUsage);
    void logMessage(const QString& message);

private Q_SLOTS:
    void onStartClicked();
    void onStopClicked();
    void onPauseClicked();
    void onModelChanged(int index);
    void onTrackerChanged(int index);
    void onInputSourceChanged();
    void onBrowseVideoClicked();
    void onOperationModeChanged();
    void onMAVLinkEnableChanged(int state);
    void onConfigureMAVLinkClicked();
    void onSaveConfigClicked();
    void onAboutClicked();
    void onTakeoffClicked();
    void onLandClicked();
    void onAltHoldClicked();
    void refreshUI();

private:
    // UI Setup methods
    void setupUI();
    void createMenuBar();
    void createMainLayout();
    void createControlPanel();
    void createVideoDisplay();
    void createStatusPanel();
    void createInputPanel(QVBoxLayout* layout);
    void createModelPanel(QVBoxLayout* layout);
    void createTrackerPanel(QVBoxLayout* layout);
    void createMAVLinkPanel(QVBoxLayout* layout);
    void createStatsPanel(QVBoxLayout* layout);
    void applyStyles();
    
    // Helper methods
    QPixmap cvMatToQPixmap(const cv::Mat& mat);
    void loadConfiguration();
    void saveConfiguration();
    void startApplication();
    void stopApplication();
    QString getFlightModeName(uint32_t mode);
    
    // UI Components - Video Display
    QLabel* videoLabel_;
    QLabel* trackingLabel_;
    QFrame* videoFrame_;
    
    // Control Buttons
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* pauseBtn_;
    QPushButton* browseVideoBtn_;
    QPushButton* configureMAVLinkBtn_;
    QPushButton* saveConfigBtn_;
    QPushButton* takeoffBtn_;
    QPushButton* landBtn_;
    QPushButton* altHoldBtn_;
    
    // Input Selection
    QRadioButton* cameraRadio_;
    QRadioButton* videoRadio_;
    QLineEdit* videoPathEdit_;
    
    // Model Selection
    QRadioButton* vehicleModelRadio_;
    QRadioButton* helmetModelRadio_;
    QRadioButton* faceModelRadio_;
    QRadioButton* colorModelRadio_;
    QRadioButton* apriltagModelRadio_;
    
    // Operation Mode
    QRadioButton* detectTrackRadio_;
    QRadioButton* detectOnlyRadio_;
    
    // Tracker Selection
    QComboBox* trackerCombo_;
    QCheckBox* showPathCheckbox_;
    
    // MAVLink Controls
    QCheckBox* mavlinkEnableCheckbox_;
    QLabel* mavlinkStatusLabel_;
    QLabel* flightModeLabel_;
    QLabel* altitudeLabel_;
    QLabel* batteryLabel_;
    QProgressBar* batteryProgressBar_;
    
    // Status Display
    QLabel* statusLabel_;
    QLabel* fpsLabel_;
    QLabel* detectionCountLabel_;
    QLabel* trackingStatusLabel_;
    QLabel* cpuUsageLabel_;
    QLabel* memoryUsageLabel_;
    QProgressBar* cpuProgressBar_;
    QProgressBar* memoryProgressBar_;
    
    // Log Display
    QTextEdit* logTextEdit_;
    
    // Application reference
    std::unique_ptr<Application> app_;
    
    // Update timer for UI refresh
    QTimer* updateTimer_;
    
    // State
    std::atomic<bool> isRunning_;
    std::atomic<bool> isPaused_;
    QString configPath_;
    
    // Statistics
    int detectionCount_;
    double currentFPS_;
    float cpuUsage_;
    float memoryUsage_;
};

#endif // MAINWINDOW_H

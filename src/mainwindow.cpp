#include "include/logger.h"
#include "include/mainwindow.h"
#include "include/application.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QApplication>
#include <QScreen>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , isRunning_(false)
    , isPaused_(false)
    , detectionCount_(0)
    , currentFPS_(0.0)
    , cpuUsage_(0.0f)
    , memoryUsage_(0.0f)
{
    setupUI();
    loadConfiguration();
    
    // Connect video frame signals to slots
    connect(this, &MainWindow::videoFrameReady, this, &MainWindow::onVideoFrameReady, Qt::QueuedConnection);
    connect(this, &MainWindow::trackingFrameReady, this, &MainWindow::onTrackingFrameReady, Qt::QueuedConnection);
    
    // Set up update timer for UI refresh
    updateTimer_ = new QTimer(this);
    connect(updateTimer_, &QTimer::timeout, this, &MainWindow::refreshUI);
    updateTimer_->start(100); // Update UI every 100ms
    
    LOG_INFO("MainWindow initialized");
}

MainWindow::~MainWindow() {
    if (app_ && isRunning_) {
        stopApplication();
    }
}

void MainWindow::setupUI() {
    setWindowTitle("AI Platform - Object Detection & Tracking");
    
    // Get screen size for initial window sizing
    QScreen *screen = QApplication::primaryScreen();
    QRect screenGeometry = screen->geometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height();
    
    // Set window size to 80% of screen
    resize(screenWidth * 0.8, screenHeight * 0.8);
    
    createMenuBar();
    createMainLayout();
    applyStyles();
}

void MainWindow::createMenuBar() {
    QMenuBar *menuBar = new QMenuBar(this);
    
    // File Menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    
    QAction *saveConfigAction = new QAction("&Save Configuration", this);
    connect(saveConfigAction, &QAction::triggered, this, &MainWindow::onSaveConfigClicked);
    fileMenu->addAction(saveConfigAction);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = new QAction("E&xit", this);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);
    
    // Help Menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    QAction *aboutAction = new QAction("&About", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAboutClicked);
    helpMenu->addAction(aboutAction);
    
    setMenuBar(menuBar);
}

void MainWindow::createMainLayout() {
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    
    // Create splitter for resizable panels
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left panel: Controls
    QWidget *controlWidget = new QWidget();
    createControlPanel();
    QVBoxLayout *controlLayout = new QVBoxLayout(controlWidget);
    
    createInputPanel(controlLayout);
    createModelPanel(controlLayout);
    createTrackerPanel(controlLayout);
    createMAVLinkPanel(controlLayout);
    
    controlLayout->addStretch();
    controlWidget->setLayout(controlLayout);
    controlWidget->setMaximumWidth(350);
    
    // Center panel: Video Display
    createVideoDisplay();
    
    // Right panel: Status and Stats
    QWidget *statusWidget = new QWidget();
    QVBoxLayout *statusLayout = new QVBoxLayout(statusWidget);
    createStatusPanel();
    createStatsPanel(statusLayout);
    statusWidget->setLayout(statusLayout);
    statusWidget->setMaximumWidth(300);
    
    // Add to splitter
    splitter->addWidget(controlWidget);
    splitter->addWidget(videoFrame_);
    splitter->addWidget(statusWidget);
    
    // Set stretch factors (center video gets most space)
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);
    splitter->setStretchFactor(2, 1);
    
    mainLayout->addWidget(splitter);
    setCentralWidget(centralWidget);
}

void MainWindow::createControlPanel() {
    // Control buttons will be added to each panel
}

void MainWindow::createInputPanel(QVBoxLayout* layout) {
    QGroupBox *inputGroup = new QGroupBox("Input Source");
    QVBoxLayout *inputLayout = new QVBoxLayout();
    
    cameraRadio_ = new QRadioButton("Camera");
    videoRadio_ = new QRadioButton("Video File");
    cameraRadio_->setChecked(true);
    
    connect(cameraRadio_, &QRadioButton::toggled, this, &MainWindow::onInputSourceChanged);
    connect(videoRadio_, &QRadioButton::toggled, this, &MainWindow::onInputSourceChanged);
    
    QHBoxLayout *videoPathLayout = new QHBoxLayout();
    videoPathEdit_ = new QLineEdit();
    videoPathEdit_->setPlaceholderText("Video file path...");
    videoPathEdit_->setEnabled(false);
    
    browseVideoBtn_ = new QPushButton("Browse");
    browseVideoBtn_->setEnabled(false);
    connect(browseVideoBtn_, &QPushButton::clicked, this, &MainWindow::onBrowseVideoClicked);
    
    videoPathLayout->addWidget(videoPathEdit_);
    videoPathLayout->addWidget(browseVideoBtn_);
    
    inputLayout->addWidget(cameraRadio_);
    inputLayout->addWidget(videoRadio_);
    inputLayout->addLayout(videoPathLayout);
    
    inputGroup->setLayout(inputLayout);
    layout->addWidget(inputGroup);
}

void MainWindow::createModelPanel(QVBoxLayout* layout) {
    QGroupBox *modelGroup = new QGroupBox("Detection Model");
    QVBoxLayout *modelLayout = new QVBoxLayout();
    
    vehicleModelRadio_ = new QRadioButton("Vehicle Detection");
    helmetModelRadio_ = new QRadioButton("Helmet Detection");
    faceModelRadio_ = new QRadioButton("Face Detection");
    colorModelRadio_ = new QRadioButton("Color Detection");
    apriltagModelRadio_ = new QRadioButton("AprilTag Detection");
    
    vehicleModelRadio_->setChecked(true);
    
    modelLayout->addWidget(vehicleModelRadio_);
    modelLayout->addWidget(helmetModelRadio_);
    modelLayout->addWidget(faceModelRadio_);
    modelLayout->addWidget(colorModelRadio_);
    modelLayout->addWidget(apriltagModelRadio_);
    
    // Operation Mode
    QGroupBox *modeGroup = new QGroupBox("Operation Mode");
    QVBoxLayout *modeLayout = new QVBoxLayout();
    
    detectTrackRadio_ = new QRadioButton("Detect + Track");
    detectOnlyRadio_ = new QRadioButton("Detect Only");
    detectTrackRadio_->setChecked(true);
    
    connect(detectTrackRadio_, &QRadioButton::toggled, this, &MainWindow::onOperationModeChanged);
    
    modeLayout->addWidget(detectTrackRadio_);
    modeLayout->addWidget(detectOnlyRadio_);
    modeGroup->setLayout(modeLayout);
    
    modelLayout->addWidget(modeGroup);
    
    modelGroup->setLayout(modelLayout);
    layout->addWidget(modelGroup);
}

void MainWindow::createTrackerPanel(QVBoxLayout* layout) {
    QGroupBox *trackerGroup = new QGroupBox("Tracker Settings");
    QVBoxLayout *trackerLayout = new QVBoxLayout();
    
    QLabel *trackerLabel = new QLabel("Tracker Type:");
    trackerCombo_ = new QComboBox();
    trackerCombo_->addItem("VitTracker");
    trackerCombo_->addItem("SiamFC++");
    trackerCombo_->addItem("CSRT");
    
    connect(trackerCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onTrackerChanged);
    
    showPathCheckbox_ = new QCheckBox("Show Tracking Path");
    showPathCheckbox_->setChecked(false);
    
    trackerLayout->addWidget(trackerLabel);
    trackerLayout->addWidget(trackerCombo_);
    trackerLayout->addWidget(showPathCheckbox_);
    
    trackerGroup->setLayout(trackerLayout);
    layout->addWidget(trackerGroup);
}

void MainWindow::createMAVLinkPanel(QVBoxLayout* layout) {
    QGroupBox *mavlinkGroup = new QGroupBox("MAVLink Control");
    QVBoxLayout *mavlinkLayout = new QVBoxLayout();
    
    mavlinkEnableCheckbox_ = new QCheckBox("Enable MAVLink");
    mavlinkEnableCheckbox_->setChecked(false);
    connect(mavlinkEnableCheckbox_, &QCheckBox::stateChanged, 
            this, &MainWindow::onMAVLinkEnableChanged);
    
    mavlinkStatusLabel_ = new QLabel("Status: Disconnected");
    flightModeLabel_ = new QLabel("Mode: --");
    altitudeLabel_ = new QLabel("Altitude: -- m");
    batteryLabel_ = new QLabel("Battery: --%");
    
    batteryProgressBar_ = new QProgressBar();
    batteryProgressBar_->setRange(0, 100);
    batteryProgressBar_->setValue(0);
    
    configureMAVLinkBtn_ = new QPushButton("Configure");
    configureMAVLinkBtn_->setEnabled(false);
    connect(configureMAVLinkBtn_, &QPushButton::clicked, 
            this, &MainWindow::onConfigureMAVLinkClicked);
    
    mavlinkLayout->addWidget(mavlinkEnableCheckbox_);
    mavlinkLayout->addWidget(mavlinkStatusLabel_);
    mavlinkLayout->addWidget(flightModeLabel_);
    mavlinkLayout->addWidget(altitudeLabel_);
    mavlinkLayout->addWidget(batteryLabel_);
    mavlinkLayout->addWidget(batteryProgressBar_);
    mavlinkLayout->addWidget(configureMAVLinkBtn_);
    
    // Flight mode button
    altHoldBtn_ = new QPushButton("AltHold Mode");
    altHoldBtn_->setEnabled(false);
    connect(altHoldBtn_, &QPushButton::clicked, this, &MainWindow::onAltHoldClicked);
    mavlinkLayout->addWidget(altHoldBtn_);
    
    // Takeoff and Land buttons
    QHBoxLayout *flightControlLayout = new QHBoxLayout();
    takeoffBtn_ = new QPushButton("Takeoff");
    landBtn_ = new QPushButton("Land");
    takeoffBtn_->setEnabled(false);
    landBtn_->setEnabled(false);
    connect(takeoffBtn_, &QPushButton::clicked, this, &MainWindow::onTakeoffClicked);
    connect(landBtn_, &QPushButton::clicked, this, &MainWindow::onLandClicked);
    flightControlLayout->addWidget(takeoffBtn_);
    flightControlLayout->addWidget(landBtn_);
    mavlinkLayout->addLayout(flightControlLayout);
    
    mavlinkGroup->setLayout(mavlinkLayout);
    layout->addWidget(mavlinkGroup);
    
    // Control Buttons
    QHBoxLayout *controlBtnLayout = new QHBoxLayout();
    
    startBtn_ = new QPushButton("Start");
    stopBtn_ = new QPushButton("Stop");
    pauseBtn_ = new QPushButton("Pause");
    
    startBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);
    pauseBtn_->setEnabled(false);
    
    connect(startBtn_, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(stopBtn_, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(pauseBtn_, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    
    controlBtnLayout->addWidget(startBtn_);
    controlBtnLayout->addWidget(stopBtn_);
    controlBtnLayout->addWidget(pauseBtn_);
    
    layout->addLayout(controlBtnLayout);
}

void MainWindow::createVideoDisplay() {
    videoFrame_ = new QFrame();
    videoFrame_->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    videoFrame_->setLineWidth(2);
    
    QVBoxLayout *mainVideoLayout = new QVBoxLayout(videoFrame_);
    
    // Create horizontal splitter for detection and tracking views
    QSplitter *videoSplitter = new QSplitter(Qt::Horizontal);
    
    // Detection frame
    QFrame *detectionFrame = new QFrame();
    detectionFrame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    QVBoxLayout *detectionLayout = new QVBoxLayout(detectionFrame);
    detectionLayout->setContentsMargins(2, 2, 2, 2);
    detectionLayout->setSpacing(2);
    
    QLabel *detectionTitle = new QLabel("Detection");
    detectionTitle->setAlignment(Qt::AlignCenter);
    detectionTitle->setStyleSheet("QLabel { font-weight: bold; color: #4a90d9; font-size: 10px; padding: 2px; }");
    detectionTitle->setMaximumHeight(18);
    
    videoLabel_ = new QLabel("No video feed");
    videoLabel_->setAlignment(Qt::AlignCenter);
    videoLabel_->setScaledContents(false);
    videoLabel_->setMinimumSize(320, 240);
    videoLabel_->setStyleSheet("QLabel { background-color: #2b2b2b; color: #888; font-size: 12px; }");
    
    detectionLayout->addWidget(detectionTitle);
    detectionLayout->addWidget(videoLabel_, 1);
    detectionFrame->setLayout(detectionLayout);
    
    // Tracking frame
    QFrame *trackingFrame = new QFrame();
    trackingFrame->setFrameStyle(QFrame::Box | QFrame::Sunken);
    QVBoxLayout *trackingLayout = new QVBoxLayout(trackingFrame);
    trackingLayout->setContentsMargins(2, 2, 2, 2);
    trackingLayout->setSpacing(2);
    
    QLabel *trackingTitle = new QLabel("Tracking");
    trackingTitle->setAlignment(Qt::AlignCenter);
    trackingTitle->setStyleSheet("QLabel { font-weight: bold; color: #4a90d9; font-size: 10px; padding: 2px; }");
    trackingTitle->setMaximumHeight(18);
    
    trackingLabel_ = new QLabel("No tracking active");
    trackingLabel_->setAlignment(Qt::AlignCenter);
    trackingLabel_->setScaledContents(false);
    trackingLabel_->setMinimumSize(320, 240);
    trackingLabel_->setStyleSheet("QLabel { background-color: #2b2b2b; color: #888; font-size: 12px; }");
    
    trackingLayout->addWidget(trackingTitle);
    trackingLayout->addWidget(trackingLabel_, 1);
    trackingFrame->setLayout(trackingLayout);
    
    // Add both frames to splitter
    videoSplitter->addWidget(detectionFrame);
    videoSplitter->addWidget(trackingFrame);
    videoSplitter->setStretchFactor(0, 1);
    videoSplitter->setStretchFactor(1, 1);
    
    mainVideoLayout->addWidget(videoSplitter);
    videoFrame_->setLayout(mainVideoLayout);
}

void MainWindow::createStatusPanel() {
    // Status panel is created in createStatsPanel
}

void MainWindow::createStatsPanel(QVBoxLayout* layout) {
    // Status Group
    QGroupBox *statusGroup = new QGroupBox("Status");
    QVBoxLayout *statusLayout = new QVBoxLayout();
    
    statusLabel_ = new QLabel("Status: Ready");
    fpsLabel_ = new QLabel("FPS: 0.0");
    detectionCountLabel_ = new QLabel("Detections: 0");
    trackingStatusLabel_ = new QLabel("Tracking: Inactive");
    
    statusLayout->addWidget(statusLabel_);
    statusLayout->addWidget(fpsLabel_);
    statusLayout->addWidget(detectionCountLabel_);
    statusLayout->addWidget(trackingStatusLabel_);
    
    statusGroup->setLayout(statusLayout);
    layout->addWidget(statusGroup);
    
    // Resource Usage Group
    QGroupBox *resourceGroup = new QGroupBox("System Resources");
    QVBoxLayout *resourceLayout = new QVBoxLayout();
    
    cpuUsageLabel_ = new QLabel("CPU: 0%");
    cpuProgressBar_ = new QProgressBar();
    cpuProgressBar_->setRange(0, 100);
    cpuProgressBar_->setValue(0);
    
    memoryUsageLabel_ = new QLabel("Memory: 0%");
    memoryProgressBar_ = new QProgressBar();
    memoryProgressBar_->setRange(0, 100);
    memoryProgressBar_->setValue(0);
    
    resourceLayout->addWidget(cpuUsageLabel_);
    resourceLayout->addWidget(cpuProgressBar_);
    resourceLayout->addWidget(memoryUsageLabel_);
    resourceLayout->addWidget(memoryProgressBar_);
    
    resourceGroup->setLayout(resourceLayout);
    layout->addWidget(resourceGroup);
    
    // Log Display
    QGroupBox *logGroup = new QGroupBox("Log Messages");
    QVBoxLayout *logLayout = new QVBoxLayout();
    
    logTextEdit_ = new QTextEdit();
    logTextEdit_->setReadOnly(true);
    logTextEdit_->setMaximumHeight(200);
    logTextEdit_->setStyleSheet("QTextEdit { font-family: monospace; font-size: 10px; }");
    
    logLayout->addWidget(logTextEdit_);
    logGroup->setLayout(logLayout);
    layout->addWidget(logGroup);
    
    layout->addStretch();
}

void MainWindow::applyStyles() {
    // Modern dark theme styling
    QString styleSheet = R"(
        QMainWindow {
            background-color: #353535;
        }
        QGroupBox {
            font-weight: bold;
            border: 1px solid #555;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: #404040;
            color: #e0e0e0;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QPushButton {
            background-color: #505050;
            color: #e0e0e0;
            border: 1px solid #666;
            border-radius: 3px;
            padding: 5px 15px;
            min-height: 25px;
        }
        QPushButton:hover {
            background-color: #606060;
        }
        QPushButton:pressed {
            background-color: #404040;
        }
        QPushButton:disabled {
            background-color: #383838;
            color: #666;
        }
        QPushButton#startBtn {
            background-color: #2d5f2d;
        }
        QPushButton#startBtn:hover {
            background-color: #3d7f3d;
        }
        QPushButton#stopBtn {
            background-color: #5f2d2d;
        }
        QPushButton#stopBtn:hover {
            background-color: #7f3d3d;
        }
        QLabel {
            color: #e0e0e0;
        }
        QRadioButton, QCheckBox {
            color: #e0e0e0;
        }
        QComboBox {
            background-color: #505050;
            color: #e0e0e0;
            border: 1px solid #666;
            border-radius: 3px;
            padding: 3px;
        }
        QLineEdit {
            background-color: #505050;
            color: #e0e0e0;
            border: 1px solid #666;
            border-radius: 3px;
            padding: 3px;
        }
        QProgressBar {
            border: 1px solid #666;
            border-radius: 3px;
            text-align: center;
            background-color: #505050;
            color: #e0e0e0;
        }
        QProgressBar::chunk {
            background-color: #4a90d9;
        }
        QTextEdit {
            background-color: #2b2b2b;
            color: #e0e0e0;
            border: 1px solid #666;
        }
    )";
    
    setStyleSheet(styleSheet);
    
    // Set object names for specific styling
    startBtn_->setObjectName("startBtn");
    stopBtn_->setObjectName("stopBtn");
}

// Slots Implementation

void MainWindow::onStartClicked() {
    LOG_INFO("Start button clicked");
    startApplication();
}

void MainWindow::onStopClicked() {
    LOG_INFO("Stop button clicked");
    stopApplication();
}

void MainWindow::onPauseClicked() {
    LOG_INFO("Pause button clicked");
    isPaused_ = !isPaused_;
    
    if (isPaused_) {
        pauseBtn_->setText("Resume");
        statusLabel_->setText("Status: Paused");
    } else {
        pauseBtn_->setText("Pause");
        statusLabel_->setText("Status: Running");
    }
}

void MainWindow::onModelChanged(int index) {
    LOG_INFO("Model changed to index: {}", index);
}

void MainWindow::onTrackerChanged(int index) {
    (void)index; // Unused parameter
    LOG_INFO("Tracker changed to: {}", trackerCombo_->currentText().toStdString());
}

void MainWindow::onInputSourceChanged() {
    bool isVideo = videoRadio_->isChecked();
    videoPathEdit_->setEnabled(isVideo);
    browseVideoBtn_->setEnabled(isVideo);
}

void MainWindow::onBrowseVideoClicked() {
    QString filename = QFileDialog::getOpenFileName(this,
        "Select Video File", "", "Video Files (*.mp4 *.avi *.mkv *.mov)");
    
    if (!filename.isEmpty()) {
        videoPathEdit_->setText(filename);
    }
}

void MainWindow::onOperationModeChanged() {
    bool trackingEnabled = detectTrackRadio_->isChecked();
    trackerCombo_->setEnabled(trackingEnabled);
    showPathCheckbox_->setEnabled(trackingEnabled);
    
    LOG_INFO("Operation mode changed - Tracking: {}", trackingEnabled ? "enabled" : "disabled");
}

void MainWindow::onMAVLinkEnableChanged(int state) {
    bool enabled = (state == Qt::Checked);
    configureMAVLinkBtn_->setEnabled(enabled);
    
    // Enable/disable flight control buttons based on MAVLink enabled state
    // They will remain enabled if MAVLink is enabled, even before connection
    altHoldBtn_->setEnabled(enabled);
    takeoffBtn_->setEnabled(enabled);
    landBtn_->setEnabled(enabled);
    
    LOG_INFO("MAVLink: {}", enabled ? "enabled" : "disabled");
}

void MainWindow::onConfigureMAVLinkClicked() {
    QMessageBox::information(this, "MAVLink Configuration", 
        "MAVLink configuration dialog would appear here.\nConfigure serial port, baud rate, etc.");
}

void MainWindow::onSaveConfigClicked() {
    saveConfiguration();
    QMessageBox::information(this, "Configuration", "Configuration saved successfully!");
}

void MainWindow::onAboutClicked() {
    QMessageBox::about(this, "About AI Platform",
        "<h2>AI Platform</h2>"
        "<p>Object Detection and Tracking System</p>"
        "<p>Version 2.0</p>"
        "<p>Raspberry Pi 5 Edition</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>YOLO Object Detection</li>"
        "<li>Multiple Tracker Support (VitTracker, SiamFC++, CSRT)</li>"
        "<li>MAVLink Drone Control</li>"
        "<li>Real-time Video Processing</li>"
        "</ul>");
}

void MainWindow::onTakeoffClicked() {
    if (!app_) {
        logMessage("Application not initialized");
        return;
    }
    
    bool ok;
    double altitude = QInputDialog::getDouble(this, "Takeoff Altitude",
                                              "Enter takeoff altitude (meters):",
                                              3.0, 0.5, 50.0, 1, &ok);
    if (ok) {
        logMessage(QString("Requesting takeoff to %1 meters...").arg(altitude));
        if (app_->takeoff(static_cast<float>(altitude))) {
            logMessage("Takeoff command sent successfully");
        } else {
            logMessage("Failed to send takeoff command");
        }
    }
}

void MainWindow::onLandClicked() {
    if (!app_) {
        logMessage("Application not initialized");
        return;
    }
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Landing",
                                   "Are you sure you want to land?",
                                   QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        logMessage("Requesting landing...");
        if (app_->land()) {
            logMessage("Land command sent successfully");
        } else {
            logMessage("Failed to send land command");
        }
    }
}

void MainWindow::onAltHoldClicked() {
    if (!app_) {
        logMessage("Application not initialized");
        return;
    }
    
    logMessage("Setting flight mode to AltHold...");
    if (app_->setFlightMode(2)) {  // 2 = AltHold mode for ArduCopter
        logMessage("AltHold mode command sent successfully");
    } else {
        logMessage("Failed to set AltHold mode");
    }
}

void MainWindow::refreshUI() {
    // Update UI elements periodically
    // This can be used to poll application state if needed
}

// Public Update Methods

void MainWindow::updateVideoFrame(const cv::Mat& frame) {
    if (frame.empty()) return;
    
    QPixmap pixmap = cvMatToQPixmap(frame);
    
    // Scale pixmap to fit label while maintaining aspect ratio
    pixmap = pixmap.scaled(videoLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    videoLabel_->setPixmap(pixmap);
}

void MainWindow::updateVideoFrameThreadSafe(const cv::Mat& frame) {
    if (frame.empty()) return;
    
    // Convert cv::Mat to QImage
    cv::Mat rgb;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    } else if (frame.channels() == 3) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    } else {
        rgb = frame.clone();
    }
    
    QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    // Deep copy to avoid data being freed
    QImage imageCopy = image.copy();
    
    // Emit signal which will be handled in the main thread
    Q_EMIT videoFrameReady(imageCopy);
}

void MainWindow::onVideoFrameReady(const QImage& image) {
    QPixmap pixmap = QPixmap::fromImage(image);
    // Scale pixmap to fit label while maintaining aspect ratio
    pixmap = pixmap.scaled(videoLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    videoLabel_->setPixmap(pixmap);
}

void MainWindow::updateTrackingFrameThreadSafe(const cv::Mat& frame) {
    if (frame.empty()) return;
    
    // Convert cv::Mat to QImage
    cv::Mat rgb;
    if (frame.channels() == 1) {
        cv::cvtColor(frame, rgb, cv::COLOR_GRAY2RGB);
    } else if (frame.channels() == 3) {
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    } else {
        rgb = frame.clone();
    }
    
    QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    // Deep copy to avoid data being freed
    QImage imageCopy = image.copy();
    
    // Emit signal which will be handled in the main thread
    Q_EMIT trackingFrameReady(imageCopy);
}

void MainWindow::onTrackingFrameReady(const QImage& image) {
    QPixmap pixmap = QPixmap::fromImage(image);
    // Scale pixmap to fit label while maintaining aspect ratio
    pixmap = pixmap.scaled(trackingLabel_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    trackingLabel_->setPixmap(pixmap);
}

void MainWindow::updateStatus(const QString& status) {
    statusLabel_->setText("Status: " + status);
}

void MainWindow::updateFPS(double fps) {
    currentFPS_ = fps;
    fpsLabel_->setText(QString("FPS: %1").arg(fps, 0, 'f', 1));
}

void MainWindow::updateDetectionStats(int detectionCount) {
    detectionCount_ = detectionCount;
    detectionCountLabel_->setText(QString("Detections: %1").arg(detectionCount));
}

void MainWindow::updateTrackingInfo(bool tracking, const QString& className, float confidence) {
    if (tracking) {
        trackingStatusLabel_->setText(
            QString("Tracking: %1 (%2%)").arg(className).arg(confidence * 100, 0, 'f', 0));
    } else {
        trackingStatusLabel_->setText("Tracking: Inactive");
    }
}

void MainWindow::updateMAVLinkStatus(bool connected, uint32_t flightMode, bool armed, 
                                    float altitude, float battery) {
    if (connected) {
        mavlinkStatusLabel_->setText("Status: Connected");
        flightModeLabel_->setText(QString("Mode: %1 %2")
            .arg(getFlightModeName(flightMode))
            .arg(armed ? "(ARMED)" : "(DISARMED)"));
        altitudeLabel_->setText(QString("Altitude: %1 m").arg(altitude, 0, 'f', 1));
        batteryLabel_->setText(QString("Battery: %1%").arg(battery, 0, 'f', 0));
        batteryProgressBar_->setValue(static_cast<int>(battery));
        
        // Color code battery level
        if (battery > 50) {
            batteryProgressBar_->setStyleSheet("QProgressBar::chunk { background-color: #4a90d9; }");
        } else if (battery > 20) {
            batteryProgressBar_->setStyleSheet("QProgressBar::chunk { background-color: #ff9900; }");
        } else {
            batteryProgressBar_->setStyleSheet("QProgressBar::chunk { background-color: #ff3333; }");
        }
        
        // Enable flight mode button when connected (doesn't need to be armed)
        altHoldBtn_->setEnabled(connected);
        
        // Enable flight control buttons when connected (arm check removed for testing)
        takeoffBtn_->setEnabled(connected);
        landBtn_->setEnabled(connected);
    } else {
        mavlinkStatusLabel_->setText("Status: Disconnected");
        flightModeLabel_->setText("Mode: --");
        altitudeLabel_->setText("Altitude: -- m");
        batteryLabel_->setText("Battery: --%");
        batteryProgressBar_->setValue(0);
        
        // Disable flight control buttons when disconnected
        altHoldBtn_->setEnabled(false);
        takeoffBtn_->setEnabled(false);
        landBtn_->setEnabled(false);
    }
}

void MainWindow::updateResourceStats(float cpuUsage, float memoryUsage) {
    cpuUsage_ = cpuUsage;
    memoryUsage_ = memoryUsage;
    
    cpuUsageLabel_->setText(QString("CPU: %1%").arg(cpuUsage, 0, 'f', 1));
    cpuProgressBar_->setValue(static_cast<int>(cpuUsage));
    
    memoryUsageLabel_->setText(QString("Memory: %1%").arg(memoryUsage, 0, 'f', 1));
    memoryProgressBar_->setValue(static_cast<int>(memoryUsage));
}

void MainWindow::logMessage(const QString& message) {
    logTextEdit_->append(message);
    
    // Auto-scroll to bottom
    QTextCursor cursor = logTextEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    logTextEdit_->setTextCursor(cursor);
    
    // Limit log size
    if (logTextEdit_->document()->lineCount() > 1000) {
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 100);
        cursor.removeSelectedText();
    }
}

// Helper Methods

QPixmap MainWindow::cvMatToQPixmap(const cv::Mat& mat) {
    cv::Mat rgb;
    
    if (mat.channels() == 1) {
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    } else if (mat.channels() == 3) {
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    } else {
        rgb = mat.clone();
    }
    
    QImage image(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    return QPixmap::fromImage(image.copy());
}

void MainWindow::loadConfiguration() {
    configPath_ = "/home/pi5/shared_folder/aiPlatform/config/config.txt";
    LOG_INFO("Loading configuration from: {}", configPath_.toStdString());
    
    // Read config file and sync UI with current settings
    std::ifstream file(configPath_.toStdString());
    if (!file.is_open()) {
        LOG_WARN("Could not open config file, using defaults");
        return;
    }
    
    std::string line;
    std::string currentSection;
    
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') continue;
        
        // Check for section headers
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Parse key=value pairs
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            
            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            // Update UI based on config
            if (currentSection == "input" && key == "input_type") {
                int inputType = std::stoi(value);
                if (inputType == 0) {
                    cameraRadio_->setChecked(true);
                } else {
                    videoRadio_->setChecked(true);
                }
            }
            else if (currentSection == "detection_model" && key == "model_type") {
                int modelType = std::stoi(value);
                switch (modelType) {
                    case 0: vehicleModelRadio_->setChecked(true); break;
                    case 1: helmetModelRadio_->setChecked(true); break;
                    case 2: faceModelRadio_->setChecked(true); break;
                    case 3: colorModelRadio_->setChecked(true); break;
                    case 4: apriltagModelRadio_->setChecked(true); break;
                    default: vehicleModelRadio_->setChecked(true); break;
                }
            }
            else if (currentSection == "general" && key == "operation_mode") {
                int opMode = std::stoi(value);
                if (opMode == 0) {
                    detectTrackRadio_->setChecked(true);
                } else {
                    detectOnlyRadio_->setChecked(true);
                }
            }
            else if (currentSection == "tracking" && key == "tracker_type") {
                int trackerType = std::stoi(value);
                trackerCombo_->setCurrentIndex(trackerType);
            }
            else if (currentSection == "visualization" && key == "show_tracking_path") {
                int showPath = std::stoi(value);
                showPathCheckbox_->setChecked(showPath == 1);
            }
            else if (currentSection == "mavlink" && key == "enabled") {
                int enabled = std::stoi(value);
                mavlinkEnableCheckbox_->setChecked(enabled == 1);
            }
        }
    }
    
    file.close();
    LOG_INFO("Configuration loaded and UI synced");
}

void MainWindow::saveConfiguration() {
    LOG_INFO("Saving configuration to: {}", configPath_.toStdString());
    
    // Read entire config file
    std::ifstream inFile(configPath_.toStdString());
    if (!inFile.is_open()) {
        LOG_ERROR("Could not open config file for reading");
        return;
    }
    
    std::vector<std::string> lines;
    std::string line;
    std::string currentSection;
    
    while (std::getline(inFile, line)) {
        std::string originalLine = line;
        
        // Trim for parsing
        std::string trimmedLine = line;
        trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t\r\n"));
        trimmedLine.erase(trimmedLine.find_last_not_of(" \t\r\n") + 1);
        
        // Track current section
        if (!trimmedLine.empty() && trimmedLine[0] == '[' && trimmedLine.back() == ']') {
            currentSection = trimmedLine.substr(1, trimmedLine.length() - 2);
            lines.push_back(originalLine);
            continue;
        }
        
        // Check if this is a key=value line
        size_t pos = trimmedLine.find('=');
        if (pos != std::string::npos && !trimmedLine.empty() && trimmedLine[0] != '#') {
            std::string key = trimmedLine.substr(0, pos);
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            
            // Update values based on UI state
            bool updated = false;
            
            if (currentSection == "input" && key == "input_type") {
                int inputType = cameraRadio_->isChecked() ? 0 : 1;
                lines.push_back(key + "=" + std::to_string(inputType));
                updated = true;
            }
            else if (currentSection == "detection_model" && key == "model_type") {
                int modelType = 0;
                if (vehicleModelRadio_->isChecked()) modelType = 0;
                else if (helmetModelRadio_->isChecked()) modelType = 1;
                else if (faceModelRadio_->isChecked()) modelType = 2;
                else if (colorModelRadio_->isChecked()) modelType = 3;
                else if (apriltagModelRadio_->isChecked()) modelType = 4;
                
                lines.push_back(key + "=" + std::to_string(modelType));
                updated = true;
            }
            else if (currentSection == "general" && key == "operation_mode") {
                int opMode = detectTrackRadio_->isChecked() ? 0 : 1;
                lines.push_back(key + "=" + std::to_string(opMode));
                updated = true;
            }
            else if (currentSection == "tracking" && key == "tracker_type") {
                int trackerType = trackerCombo_->currentIndex();
                lines.push_back(key + "=" + std::to_string(trackerType));
                updated = true;
            }
            else if (currentSection == "visualization" && key == "show_tracking_path") {
                int showPath = showPathCheckbox_->isChecked() ? 1 : 0;
                lines.push_back(key + "=" + std::to_string(showPath));
                updated = true;
            }
            else if (currentSection == "mavlink" && key == "enabled") {
                int enabled = mavlinkEnableCheckbox_->isChecked() ? 1 : 0;
                lines.push_back(key + "=" + std::to_string(enabled));
                updated = true;
            }
            
            if (!updated) {
                lines.push_back(originalLine);
            }
        } else {
            lines.push_back(originalLine);
        }
    }
    
    inFile.close();
    
    // Write back to file
    std::ofstream outFile(configPath_.toStdString());
    if (!outFile.is_open()) {
        LOG_ERROR("Could not open config file for writing");
        return;
    }
    
    for (const auto& l : lines) {
        outFile << l << "\n";
    }
    
    outFile.close();
    LOG_INFO("Configuration saved successfully");
}

void MainWindow::startApplication() {
    if (isRunning_) {
        LOG_WARN("Application already running");
        return;
    }
    
    // Save current UI settings to config file before starting
    saveConfiguration();
    
    // Create and initialize application
    app_ = std::make_unique<Application>();
    app_->setMainWindow(this);
    
    if (!app_->initialize(configPath_.toStdString())) {
        QMessageBox::critical(this, "Error", "Failed to initialize application");
        LOG_ERROR("Failed to initialize application");
        app_.reset();
        return;
    }
    
    // Start application threads
    app_->startThreads();
    
    isRunning_ = true;
    startBtn_->setEnabled(false);
    stopBtn_->setEnabled(true);
    pauseBtn_->setEnabled(true);
    statusLabel_->setText("Status: Running");
    
    LOG_INFO("Application started");
}

void MainWindow::stopApplication() {
    if (!isRunning_) {
        LOG_WARN("Application not running");
        return;
    }
    
    // Stop the application
    if (app_) {
        app_->stop();
        app_->stopThreads();
        app_.reset();
    }
    
    isRunning_ = false;
    isPaused_ = false;
    startBtn_->setEnabled(true);
    stopBtn_->setEnabled(false);
    pauseBtn_->setEnabled(false);
    pauseBtn_->setText("Pause");
    statusLabel_->setText("Status: Stopped");
    videoLabel_->clear();
    videoLabel_->setText("No video feed");
    trackingLabel_->clear();
    trackingLabel_->setText("No tracking active");
    
    LOG_INFO("Application stopped");
}

QString MainWindow::getFlightModeName(uint32_t mode) {
    // ArduCopter flight modes
    switch(mode) {
        case 0: return "STABILIZE";
        case 1: return "ACRO";
        case 2: return "ALT_HOLD";
        case 3: return "AUTO";
        case 4: return "GUIDED";
        case 5: return "LOITER";
        case 6: return "RTL";
        case 7: return "CIRCLE";
        case 9: return "LAND";
        case 16: return "POSHOLD";
        case 17: return "BRAKE";
        default: return QString("UNKNOWN(%1)").arg(mode);
    }
}

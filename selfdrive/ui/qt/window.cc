#include "selfdrive/ui/qt/window.h"

#include <QFontDatabase>

#include "selfdrive/hardware/hw.h"
#include "selfdrive/ui/qt/widgets/scrollview.h"

Scroll::Scroll(QWidget *parent) : QAbstractScrollArea(parent) {

}


// TestLabel

TestLabel::TestLabel(int idx, const QString &text, QWidget *parent) : QLabel(text, parent) {
  // setAutoFillBackground(true);
  // setPalette(Qt::blue);
  // setAttribute(Qt::WA_OpaquePaintEvent);
      // setAttribute(Qt::WA_OpaquePaintEvent);
    // setAttribute(Qt::WA_StaticContents);
  id_ = idx;
}

QSize TestLabel::sizeHint() const {
  QSize sz = QWidget::sizeHint();
  printf("TestLabel::sizeHint %d (%d %d)\n", id_, sz.width(), sz.height());
  return sz;
}

void TestLabel::paintEvent(QPaintEvent* e) {
  // printf("TestLabel::paintEvent %d (%d %d)\n", id_, e->rect().width(), e->rect().height());
  QLabel::paintEvent(e);
}

TestWidget::TestWidget(QWidget *parent) : QWidget(parent) {
   setAutoFillBackground(true);
   QPalette pal(Qt::red);
    setBackgroundRole(QPalette::Button);
    // setWindowFlags(Qt::FramelessWindowHint);
    // setAttribute(Qt::WA_OpaquePaintEvent);
    // setAttribute(Qt::WA_StaticContents);
  // QVBoxLayout *v = new QVBoxLayout(this);
  // for (int i = 0; i < 80; ++i) {
  //   v->addWidget(new TestLabel(i, QString("label%1").arg(i), this));
  // }
  setFixedSize(500, 3042);
  QTimer *t = new QTimer(this);
  t->start(100);
  t->callOnTimeout([=]() {
    this->scroll(0, -10);
  });
}

// TestWidget

QSize TestWidget::sizeHint() const {
  QSize sz = QWidget::sizeHint();
  printf("TestWidget::sizeHint %d %d\n", sz.width(), sz.height());
  return {500, 3042};
}

void TestWidget::paintEvent(QPaintEvent* e) {
  printf("TestWidget::paintEvent %d %d\n", e->rect().width(), e->rect().height());
}


QSize MainWindow::sizeHint() const {
  QSize sz = QWidget::sizeHint();
  printf("MainWindow::sizeHint %d %d\n", sz.width(), sz.height());
  return sz;
}

void MainWindow::paintEvent(QPaintEvent* e) {
  printf("MainWindow::paintEvent %d %d\n", e->rect().width(), e->rect().height());
}

// MainWindow
MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  // setAutoFillBackground(true);
  // setPalette(Qt::white);
  // auto fmt = QSurfaceFormat::defaultFormat();
  // printf("fmt %d %d \n**\n", fmt.renderableType(), fmt.swapBehavior());
  // return;
  // setAttribute(Qt::WA_TranslucentBackground);
  // setWindowFlags(Qt::FramelessWindowHint);
  {
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setMargin(50);
    TestWidget *w = new TestWidget(this);
    v->addWidget(w);
    // ScrollView *panel_frame = new ScrollView(w, this);
    // v->addWidget(panel_frame);
    return;
  }
  main_layout = new QStackedLayout(this);
  main_layout->setMargin(0);

  onboardingWindow = new OnboardingWindow(this);
  main_layout->addWidget(onboardingWindow);
  QObject::connect(onboardingWindow, &OnboardingWindow::onboardingDone, [=]() {
    main_layout->setCurrentWidget(homeWindow);
  });

  homeWindow = new HomeWindow(this);
  main_layout->addWidget(homeWindow);
  QObject::connect(homeWindow, &HomeWindow::openSettings, this, &MainWindow::openSettings);
  QObject::connect(homeWindow, &HomeWindow::closeSettings, this, &MainWindow::closeSettings);
  QObject::connect(&qs, &QUIState::uiUpdate, homeWindow, &HomeWindow::update);
  QObject::connect(&qs, &QUIState::offroadTransition, homeWindow, &HomeWindow::offroadTransition);
  QObject::connect(&qs, &QUIState::offroadTransition, homeWindow, &HomeWindow::offroadTransitionSignal);
  QObject::connect(&device, &Device::displayPowerChanged, homeWindow, &HomeWindow::displayPowerChanged);

  settingsWindow = new SettingsWindow(this);
  main_layout->addWidget(settingsWindow);
  QObject::connect(settingsWindow, &SettingsWindow::closeSettings, this, &MainWindow::closeSettings);
  QObject::connect(&qs, &QUIState::offroadTransition, settingsWindow, &SettingsWindow::offroadTransition);
  QObject::connect(settingsWindow, &SettingsWindow::reviewTrainingGuide, [=]() {
    main_layout->setCurrentWidget(onboardingWindow);
  });
  QObject::connect(settingsWindow, &SettingsWindow::showDriverView, [=] {
    homeWindow->showDriverView(true);
  });

  device.setAwake(true, true);
  QObject::connect(&qs, &QUIState::uiUpdate, &device, &Device::update);
  QObject::connect(&qs, &QUIState::offroadTransition, [=](bool offroad) {
    if (!offroad) {
      closeSettings();
    }
  });
  QObject::connect(&device, &Device::displayPowerChanged, [=]() {
     if(main_layout->currentWidget() != onboardingWindow) {
       closeSettings();
     }
  });

  // load fonts
  QFontDatabase::addApplicationFont("../assets/fonts/opensans_regular.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/opensans_bold.ttf");
  QFontDatabase::addApplicationFont("../assets/fonts/opensans_semibold.ttf");

  // no outline to prevent the focus rectangle
  setStyleSheet(R"(
    * {
      font-family: Inter;
      outline: none;
    }
  )");
}

void MainWindow::openSettings() {
  main_layout->setCurrentWidget(settingsWindow);
}

void MainWindow::closeSettings() {
  main_layout->setCurrentWidget(homeWindow);

  if (QUIState::ui_state.scene.started) {
    emit homeWindow->showSidebar(false);
  }
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
  // wake screen on tap
  // if (event->type() == QEvent::MouseButtonPress) {
  //   device.setAwake(true, true);
  // }

#ifdef QCOM
  // filter out touches while in android activity
  // const static QSet<QEvent::Type> filter_events({QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::TouchBegin, QEvent::TouchUpdate, QEvent::TouchEnd});
  // if (HardwareEon::launched_activity && filter_events.contains(event->type())) {
  //   HardwareEon::check_activity();
  //   if (HardwareEon::launched_activity) {
  //     return true;
  //   }
  // }
#endif
  // return false;
  return QWidget::eventFilter(obj, event);
}

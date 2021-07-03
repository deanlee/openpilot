#include "selfdrive/ui/qt/window.h"

#include <QDebug>
#include <QFontDatabase>
#include <QPainter>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QScrollerProperties>
#include <QScroller>

#include "selfdrive/hardware/hw.h"

TestWidget2::TestWidget2(QWidget *parent) : QListWidget(parent) {
  QScroller *scroller = QScroller::scroller(this->viewport());
  QScrollerProperties sp = scroller->scrollerProperties();

  sp.setScrollMetric(QScrollerProperties::FrameRate, QScrollerProperties::Fps30);
  sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  sp.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
  sp.setScrollMetric(QScrollerProperties::DragStartDistance, 0.001);
  sp.setScrollMetric(QScrollerProperties::ScrollingCurve, QEasingCurve::Linear);

  scroller->grabGesture(this->viewport(), QScroller::LeftMouseButtonGesture);
  scroller->setScrollerProperties(sp);

}
void TestWidget2::paintEvent(QPaintEvent* e) {
  qInfo() << e->rect();// << p.paintEngine()->type();
}
TestWidget::TestWidget(QWidget *parent) : QWidget(parent) {
  QPalette pal = palette();
  // //  w->setWindowFlags(Qt::FramelessWindowHint);
  // pal.setBrush(QPalette::Background, QBrush(QColor(255, 255, 0)));
  // setAutoFillBackground(true);
  // setPalette(pal);
  // setWindowFlags(Qt::FramelessWindowHint);
  
   setPalette(Qt::transparent);
    setAttribute( Qt::WA_TranslucentBackground, true );
    setAttribute( Qt::WA_OpaquePaintEvent, true );
    setAutoFillBackground(false);
    setStyleSheet("QWidget{background-color: transparent;}");
    setAttribute(Qt::WA_NoSystemBackground);

  //  setAttribute(Qt::WA_OpaquePaintEvent);
  QVBoxLayout *l2 = new QVBoxLayout(this);
  for (int i = 0; i < 70; ++i) {
    QLabel *w = new QLabel("test");
    // w->setAttribute(Qt::WA_OpaquePaintEvent);;
    // QPalette pal = w->palette();
    //  w->setWindowFlags(Qt::FramelessWindowHint);
    // pal.setBrush(QPalette::Background, QBrush(QColor(0, 255, 0)));
    // w->setAutoFillBackground(true);
    // w->setPalette(pal);
    l2->addWidget(w);
  }
  // setMinimumSize(QSize(1024, 3048));
}
void TestWidget::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  qInfo() << e->rect() << p.paintEngine()->type();
  // assert(p.paintEngine()->type() == QPaintEngine::OpenGL2);
  // QWidget::paintEvent(e);
}

void printWindowAttr(QWidget *w, const QString &text) {
  qInfo() << "******" << text << "**************";
  qInfo() << "Qt::WA_OpaquePaintEvent" << w->testAttribute(Qt::WA_OpaquePaintEvent);
  qInfo() << "Qt::WA_NoSystemBackground" << w->testAttribute(Qt::WA_NoSystemBackground);
  qInfo() << "Qt::WA_TranslucentBackground" << w->testAttribute(Qt::WA_TranslucentBackground);
  qInfo() << "Qt::WA_SetPalette" << w->testAttribute(Qt::WA_SetPalette);
  qInfo() << "Qt::WA_SetStyle" << w->testAttribute(Qt::WA_SetStyle);
  qInfo() << "Qt::WA_StyleSheet" << w->testAttribute(Qt::WA_StyleSheet);
  qInfo() << "Qt::WA_StyleSheetTarget" << w->testAttribute(Qt::WA_StyleSheetTarget);
  qInfo() << "Qt::WA_WindowPropagation" << w->testAttribute(Qt::WA_WindowPropagation);
  qInfo() << "Qt::WA_StyledBackground" << w->testAttribute(Qt::WA_StyledBackground);
  qInfo() << "Qt::WA_NativeWindow" << w->testAttribute(Qt::WA_NativeWindow);
  qInfo() << "Qt::WA_NativeWindow" << w->testAttribute(Qt::WA_NativeWindow);
}

MainWindow::MainWindow(QWidget *parent) : QWidget(parent) {
  // qInfo() << QSurfaceFormat::defaultFormat();
  // setStyleSheet("background-color:black;color:white;");
  // setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
  // {
  //   QPalette pal = palette();
  //   //  w->setWindowFlags(Qt::FramelessWindowHint);
  //   pal.setBrush(QPalette::Background, QBrush(QColor(0, 0, 255)));
  //   setAutoFillBackground(true);
  //   setPalette(pal);
  // }
    // setAttribute(Qt::WA_OpaquePaintEvent);
  // }
//   TestWidget2 *w = new TestWidget2();
//   for (int i = 0; i< 70; ++i) {
//     w->addItem("test");
//   }
//   QVBoxLayout *l = new QVBoxLayout(this);
//   l->addWidget(w);
//   return;
// }
  // setAttribute(Qt::WA_NoSystemBackground);
  // setAttribute(Qt::WA_TranslucentBackground);
  // // setAttribute(Qt::WA_PaintOnScreen);

  // QPalette palette = this->palette();
  // palette.setBrush(QPalette::Background,QBrush(QColor(255,0, 0)));
  // setAutoFillBackground(true);
  // setPalette(palette);

  // transparent background
  // setAttribute(Qt::WA_TranslucentBackground);
  // setStyleSheet("background:transparent;");

  // no window decorations
  // setWindowFlags(Qt::FramelessWindowHint);

  // setAttribute(Qt::WA_TransparentForMouseEvents);
  //  device.setAwake(true, true);
  // setAttribute(Qt::WA_PaintOnScreen);
  //  setAutoFillBackground(true);
  //  setBackgroundRole(QPalette::Shadow);
   
  //  QVBoxLayout *l = new QVBoxLayout(this);
  // setAttribute(Qt::WA_NoSystemBackground);
  // setAttribute(Qt::WA_OpaquePaintEvent);
  // printWindowAttr(this, "main");
  qInfo() << QSurfaceFormat::defaultFormat();
  TestWidget *w = new TestWidget;
  // printWindowAttr(w, "test");
  // w->setAttribute(Qt::WA_OpaquePaintEvent);
  // QPalette palette = w->palette();
  // palette.setBrush(QPalette::Window,QBrush(QColor(61,61,61)));
  // w->setAutoFillBackground(true);
  // w->setPalette(palette);
  // w->setWindowFlags(Qt::FramelessWindowHint);
  // w->setAutoFillBackground(false);
  // w->setAttribute(Qt::WA_OpaquePaintEvent);
  // w->setAttribute(Qt::WA_NoSystemBackground);
  // w->setAttribute(Qt::WA_NoBackground);
  // w->setAttribute(Qt::WA_StyledBackground, false);
  // w->setAutoFillBackground(true);
  //  w->setAttribute(Qt::WA_NoSystemBackground);
  // w->setAttribute(Qt::WA_OpaquePaintEvent);;
  ScrollView *panel_frame = new ScrollView(w);
  // printWindowAttr(panel_frame, "scroll");
  {
    // QPalette pal = panel_frame->palette();
    // //  w->setWindowFlags(Qt::FramelessWindowHint);
    // pal.setBrush(QPalette::Background, QBrush(QColor(0, 255, 0)));
    // panel_frame->setAutoFillBackground(true);
    // panel_frame->setPalette(pal);
  }
  //  panel_frame->setAutoFillBackground(true);
  //  panel_frame->setBackgroundRole(QPalette::Window);

  //  panel_frame->setAttribute(Qt::WA_OpaquePaintEvent);
  QHBoxLayout *pLayout = new QHBoxLayout;
  pLayout->addWidget(panel_frame);
  pLayout->setMargin(0);
  pLayout->setSpacing(0);
  setLayout(pLayout);
  //  l->addWidget(panel_frame);
  //  l->addWidget(w);
  //    setStyleSheet(R"(
  //   * {
  //     font-size:50px;
  //     font-family: Inter;
  //     outline: none;
  //   }
  // )");
  return;

  //  setAttribute(Qt::WA_OpaquePaintEvent);
  //  setAttribute(Qt::WA_PaintOnScreen);
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
    if (main_layout->currentWidget() != onboardingWindow) {
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
  if (event->type() == QEvent::MouseButtonPress) {
    device.setAwake(true, true);
  }

#ifdef QCOM
  // filter out touches while in android activity
  const static QSet<QEvent::Type> filter_events({QEvent::MouseButtonPress, QEvent::MouseMove, QEvent::TouchBegin, QEvent::TouchUpdate, QEvent::TouchEnd});
  if (HardwareEon::launched_activity && filter_events.contains(event->type())) {
    HardwareEon::check_activity();
    if (HardwareEon::launched_activity) {
      return true;
    }
  }
#endif
  return false;
}

void MainWindow::paintEvent(QPaintEvent *e) {
  qInfo() << "MainWindow::paintEvent";
  // QWidget::paintEvent(e);
  // assert(0);
}
#pragma once

#include <QStackedLayout>
#include <QWidget>
#include <QListWidget>

#include "selfdrive/ui/qt/home.h"
#include "selfdrive/ui/qt/offroad/onboarding.h"
#include "selfdrive/ui/qt/offroad/settings.h"
class TestWidget2 : public QListWidget {
  Q_OBJECT
  public:
  TestWidget2(QWidget *parent = 0);
  void paintEvent(QPaintEvent*) override;
};
class TestWidget :public QWidget {
Q_OBJECT
public:
TestWidget(QWidget *parent=0);
void paintEvent(QPaintEvent*) override;
};
class MainWindow : public QWidget {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);

private:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void openSettings();
  void closeSettings();
  void paintEvent(QPaintEvent*) override;

  Device device;
  QUIState qs;

  QStackedLayout *main_layout;
  HomeWindow *homeWindow;
  SettingsWindow *settingsWindow;
  OnboardingWindow *onboardingWindow;
};

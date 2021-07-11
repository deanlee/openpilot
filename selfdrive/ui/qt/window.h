#pragma once

#include <QStackedLayout>
#include <QWidget>
#include <QLabel>

#include "selfdrive/ui/qt/home.h"
#include "selfdrive/ui/qt/offroad/onboarding.h"
#include "selfdrive/ui/qt/offroad/settings.h"
class TestLabel : public QLabel {
  Q_OBJECT
public:
  explicit TestLabel(int idx, const QString &text, QWidget *parent = 0);
  QSize sizeHint() const override;
  void paintEvent(QPaintEvent*) override;  
  int id_;
};

class TestWidget : public QWidget {
  Q_OBJECT
public:
  explicit TestWidget(QWidget *parent = 0);
  QSize sizeHint() const override;
  void paintEvent(QPaintEvent*) override;  
};

class MainWindow : public QWidget {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  QSize sizeHint() const override;
  void paintEvent(QPaintEvent*) override;  

private:
  bool eventFilter(QObject *obj, QEvent *event) override;
  void openSettings();
  void closeSettings();

  Device device;
  QUIState qs;

  QStackedLayout *main_layout;
  HomeWindow *homeWindow;
  SettingsWindow *settingsWindow;
  OnboardingWindow *onboardingWindow;
};

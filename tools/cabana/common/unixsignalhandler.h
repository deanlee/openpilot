#pragma once

#include <QSocketNotifier>

class UnixSignalHandler : public QObject {
  Q_OBJECT

public:
  UnixSignalHandler(QObject *parent = nullptr);
  ~UnixSignalHandler();
  static void signalHandler(int s);

public slots:
  void handleSigTerm();

private:
  inline static int sig_fd[2] = {};
  QSocketNotifier *sn;
};

#include "tools/cabana/common/unixsignalhandler.h"

#include <sys/socket.h>
#include <unistd.h>

#include <QApplication>
#include <csignal>

UnixSignalHandler::UnixSignalHandler(QObject *parent) : QObject(nullptr) {
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sig_fd)) {
    qFatal("Couldn't create TERM socketpair");
  }

  sn = new QSocketNotifier(sig_fd[1], QSocketNotifier::Read, this);
  connect(sn, &QSocketNotifier::activated, this, &UnixSignalHandler::handleSigTerm);
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, UnixSignalHandler::signalHandler);
}

UnixSignalHandler::~UnixSignalHandler() {
  ::close(sig_fd[0]);
  ::close(sig_fd[1]);
}

void UnixSignalHandler::signalHandler(int s) {
  ::write(sig_fd[0], &s, sizeof(s));
}

void UnixSignalHandler::handleSigTerm() {
  sn->setEnabled(false);
  int tmp;
  ::read(sig_fd[1], &tmp, sizeof(tmp));

  printf("\nexiting...\n");
  qApp->closeAllWindows();
  qApp->exit();
}

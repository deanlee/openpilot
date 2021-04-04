#pragma once

#include <QString>
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QProgressBar>
#include <curl/curl.h>

class Setup : public QStackedWidget {
  Q_OBJECT

public:
  explicit Setup(QWidget *parent = 0);

private:
  QWidget *getting_started();
  QWidget *network_setup();
  QWidget *software_selection();
  QWidget *custom_software();
  QWidget *downloading();
  QWidget *download_failed();

  QWidget *build_page(QString title, QWidget *content, bool next, bool prev);
  int download_file_xferinfo(curl_off_t dltotal, curl_off_t dlno, curl_off_t ultotal, curl_off_t ulnow);

  QProgressBar *progress_bar;

signals:
  void downloadFailed();

public slots:
  void nextPage();
  void prevPage();
  void download(QString url);
};

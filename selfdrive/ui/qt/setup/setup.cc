#include <stdio.h>
#include <stdlib.h>

#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>

#include "setup.hpp"
#include "offroad/networking.hpp"
#include "widgets/input.hpp"
#include "qt_window.hpp"

#define USER_AGENT "AGNOSSetup-0.1"

int Setup::download_file_xferinfo(curl_off_t dltotal, curl_off_t dlno, curl_off_t ultotal, curl_off_t ulnow) {
  if (dltotal != 0) {
    float progress_frac = ((float)dlno / dltotal) * 100;
    progress_bar->setValue(progress_frac);
    progress_bar->repaint();
  }
  return 0;
};


size_t download_file_write(void *ptr, size_t size, size_t nmeb, void *up) {
  return fwrite(ptr, size, nmeb, (FILE *)up);
}

void Setup::download(QString url) {
  QCoreApplication::processEvents(QEventLoop::AllEvents, 1000);
  setCurrentIndex(count() - 2);

  CURL *curl = curl_easy_init();
  if (!curl) {
    emit downloadFailed();
  }

  char tmpfile[] = "/tmp/installer_XXXXXX";
  FILE *fp = fdopen(mkstemp(tmpfile), "w");

  int tries = 4;
  bool ret = false;
  long last_resume_from = 0;
  std::string url_string = url.toStdString();
  while (true) {
    long resume_from = ftell(fp);

    curl_easy_setopt(curl, CURLOPT_URL, url_string.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 0);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(curl, CURLOPT_RESUME_FROM, resume_from);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_file_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &Setup::download_file_xferinfo);

    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // double content_length = 0.0;
    // curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &content_length);

    qInfo() << QString("download %1 res %2, code %3, resume from %4\n").arg(url).arg(res).arg(response_code).arg(resume_from);
    if (res == CURLE_OK) {
      ret = true;
      break;
    } else if (res == CURLE_HTTP_RETURNED_ERROR && response_code == 416) {
      // failed because the file is already complete?
      ret = true;
      break;
    } else if (resume_from == last_resume_from) {
      // failed and dind't make make forward progress. only retry a couple times
      tries--;
      if (tries <= 0) {
        break;
      }
    }
    last_resume_from = resume_from;
  }

  if (!ret) {
    emit downloadFailed();
  }
  curl_easy_cleanup(curl);
  fclose(fp);

  rename(tmpfile, "/tmp/installer");
}

QLabel * title_label(QString text) {
  QLabel *l = new QLabel(text);
  l->setStyleSheet(R"(
    font-size: 100px;
    font-weight: 500;
  )");
  return l;
}

QWidget * Setup::build_page(QString title, QWidget *content, bool next, bool prev) {
  QVBoxLayout *main_layout = new QVBoxLayout();
  main_layout->setMargin(50);
  main_layout->addWidget(title_label(title), 0, Qt::AlignLeft | Qt::AlignTop);

  main_layout->addWidget(content);

  QHBoxLayout *nav_layout = new QHBoxLayout();

  if (prev) {
    QPushButton *back_btn = new QPushButton("Back");
    nav_layout->addWidget(back_btn, 1, Qt::AlignBottom | Qt::AlignLeft);
    QObject::connect(back_btn, SIGNAL(released()), this, SLOT(prevPage()));
  }

  if (next) {
    QPushButton *continue_btn = new QPushButton("Continue");
    nav_layout->addWidget(continue_btn, 0, Qt::AlignBottom | Qt::AlignRight);
    QObject::connect(continue_btn, SIGNAL(released()), this, SLOT(nextPage()));
  }

  main_layout->addLayout(nav_layout, 0);

  QWidget *widget = new QWidget();
  widget->setLayout(main_layout);
  return widget;
}

QWidget * Setup::getting_started() {
  QLabel *body = new QLabel("Before we get on the road, let's finish\ninstallation and cover some details.");
  body->setAlignment(Qt::AlignHCenter);
  body->setStyleSheet(R"(font-size: 80px;)");
  return build_page("Getting Started", body, true, false);
}

QWidget * Setup::network_setup() {
  Networking *wifi = new Networking(this, false);
  return build_page("Connect to WiFi", wifi, true, true);
}

QWidget * Setup::software_selection() {
  QVBoxLayout *main_layout = new QVBoxLayout();

  QPushButton *dashcam_btn = new QPushButton("Dashcam");
  main_layout->addWidget(dashcam_btn);
  QObject::connect(dashcam_btn, &QPushButton::released, this, [=]() {
    this->download("https://dashcam.comma.ai");
  });

  main_layout->addSpacing(50);

  QPushButton *custom_btn = new QPushButton("Custom");
  main_layout->addWidget(custom_btn);
  QObject::connect(custom_btn, &QPushButton::released, this, [=]() {
    QString input_url = InputDialog::getText("Enter URL");
    if (input_url.size()) {
      this->download(input_url);
    }
  });

  QWidget *widget = new QWidget();
  widget->setLayout(main_layout);
  return build_page("Choose Software", widget, false, true);
}

QWidget * Setup::downloading() {
  QVBoxLayout *main_layout = new QVBoxLayout();
  main_layout->addStretch();
  main_layout->addWidget(title_label("Downloading..."), 0, Qt::AlignCenter);

  progress_bar = new QProgressBar();
  progress_bar->setRange(5, 100);
  progress_bar->setTextVisible(false);
  progress_bar->setFixedHeight(25);
  main_layout->addWidget(progress_bar);
  main_layout->addStretch();

  QWidget *widget = new QWidget();
  widget->setLayout(main_layout);
  return widget;
}

QWidget * Setup::download_failed() {
  QVBoxLayout *main_layout = new QVBoxLayout();
  main_layout->setContentsMargins(50, 50, 50, 50);
  main_layout->addWidget(title_label("Download Failed"), 0, Qt::AlignLeft | Qt::AlignTop);

  QLabel *body = new QLabel("Ensure the entered URL is valid, and the device's network connection is good.");
  body->setWordWrap(true);
  body->setAlignment(Qt::AlignHCenter);
  body->setStyleSheet(R"(font-size: 80px;)");
  main_layout->addWidget(body);

  QHBoxLayout *nav_layout = new QHBoxLayout();

  QPushButton *reboot_btn = new QPushButton("Reboot");
  nav_layout->addWidget(reboot_btn, 0, Qt::AlignBottom | Qt::AlignLeft);
  QObject::connect(reboot_btn, &QPushButton::released, this, [=]() {
#ifdef QCOM2
    std::system("sudo reboot");
#endif
  });

  QPushButton *restart_btn = new QPushButton("Start over");
  nav_layout->addWidget(restart_btn, 0, Qt::AlignBottom | Qt::AlignRight);
  QObject::connect(restart_btn, &QPushButton::released, this, [=]() {
    setCurrentIndex(0);
  });

  main_layout->addLayout(nav_layout, 0);

  QWidget *widget = new QWidget();
  widget->setLayout(main_layout);
  return widget;
}

void Setup::prevPage() {
  setCurrentIndex(currentIndex() - 1);
}

void Setup::nextPage() {
  setCurrentIndex(currentIndex() + 1);
}

Setup::Setup(QWidget *parent) {
  addWidget(getting_started());
  addWidget(network_setup());
  addWidget(software_selection());
  addWidget(downloading());
  addWidget(download_failed());

  QObject::connect(this, SIGNAL(downloadFailed()), this, SLOT(nextPage()));

  setStyleSheet(R"(
    * {
      font-family: Inter;
      color: white;
      background-color: black;
    }
    QPushButton {
      padding: 50px;
      padding-right: 100px;
      padding-left: 100px;
      border: 7px solid white;
      border-radius: 20px;
      font-size: 50px;
    }
    QProgressBar {
      background-color: #373737;
      width: 1000px;
      border solid white;
      border-radius: 10px;
    }
    QProgressBar::chunk {
      border-radius: 10px;
      background-color: white;
    }
  )");
}

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);
  Setup setup;
  setMainWindow(&setup);
  return a.exec();
}

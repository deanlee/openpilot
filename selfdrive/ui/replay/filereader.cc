#include "selfdrive/ui/replay/filereader.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>

#include <bzlib.h>

FileReader::FileReader(const QString &fn, QObject *parent) : url_(fn), QObject(parent) {}

void FileReader::read() {
  if (url_.isLocalFile()) {
    QFile file(url_.toLocalFile());
    if (file.open(QIODevice::ReadOnly)) {
      emit finished(file.readAll());
    } else {
      emit failed(QString("Failed to read file %1").arg(url_.toString()));
    }
  } else {
    startHttpRequest();
  }
}

void FileReader::startHttpRequest() {
  QNetworkAccessManager *qnam = new QNetworkAccessManager(this);
  QNetworkRequest request(url_);
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  reply_ = qnam->get(request);
  connect(reply_, &QNetworkReply::finished, [=]() {
    if (!reply_->error()) {
      emit finished(reply_->readAll());
    } else {
      emit failed(reply_->errorString());
    }
    reply_->deleteLater();
    reply_ = nullptr;
  });
}

void FileReader::abort() {
  if (reply_) reply_->abort();
}


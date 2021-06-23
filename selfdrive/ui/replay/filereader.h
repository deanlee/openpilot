#pragma once

#include <unordered_map>
#include <vector>

#include <QElapsedTimer>
#include <QString>
#include <QThread>
#include <QUrl>
class QNetworkReply;

class FileReader : public QObject {
  Q_OBJECT

public:
  FileReader(const QString &fn, QObject *parent = nullptr);
  void read();
  void abort();

signals:
  void finished(const QByteArray &dat);
  void failed(const QString &err);

private:
  void startHttpRequest();
  QNetworkReply *reply_ = nullptr;
  QUrl url_;
};

#pragma once

#include <QObject>

#include "selfdrive/common/params.h"

class QTimer;
class HttpRequest;

class RequestRepeater : public QObject {
  Q_OBJECT

public:
  RequestRepeater(QObject *parent, const QString &requestURL, const QString &cacheKey = "", int period = 0);

signals:
  void receivedResponse(const QString &response);
  void failedResponse(const QString &errorString);
  // void timeoutResponse(const QString &errorString);

private:
  Params params;
  QTimer *timer;
  QString prevResp;
  HttpRequest *httpRequest;
};

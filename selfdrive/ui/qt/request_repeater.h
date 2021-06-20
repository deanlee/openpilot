#pragma once

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/api.h"

class QTimer;
class RequestRepeater : public HttpRequest {
public:
  RequestRepeater(QObject *parent, const QString &requestURL, const QString &cacheKey = "", int period = 0);

private:
  Params params;
  QTimer *timer;
  QString prevResp;
};

#include <QTimer>

#include "selfdrive/ui/qt/request_repeater.h"
#include "selfdrive/ui/qt/api.h"
#include "selfdrive/ui/ui.h"

RequestRepeater::RequestRepeater(QObject *parent, const QString &requestURL, const QString &cacheKey,
                                 int period) : QObject(parent) {
  httpRequest = new HttpRequest(this, requestURL);
  QObject::connect(httpRequest, &HttpRequest::receivedResponse, [=](const QString &resp) {
    if (resp != prevResp) {
      prevResp = resp;
      if (!prevResp.isEmpty()) {
        params.put(cacheKey.toStdString(), resp.toStdString());
      }
      emit receivedResponse(resp);
    }
  });
  QObject::connect(httpRequest, &HttpRequest::failedResponse, [=](const QString &err) {
    if (!prevResp.isEmpty()) {
      prevResp = "";
      if (!!cacheKey.isEmpty()) {
        params.remove(cacheKey.toStdString());
      }
    }
    emit failedResponse(err);
  });

  timer = new QTimer(this);
  timer->setTimerType(Qt::VeryCoarseTimer);
  QObject::connect(timer, &QTimer::timeout, [=]() {
    // TODO: find a better way to do this.
    if (!QUIState::ui_state.scene.started && QUIState::ui_state.awake && !httpRequest->isRunning()) {
      httpRequest->sendRequest(requestURL);
    }
  });
  timer->start(period * 1000);

  if (!cacheKey.isEmpty()) {
    prevResp = QString::fromStdString(params.get(cacheKey.toStdString()));
    if (!prevResp.isEmpty()) {
      QTimer::singleShot(0, [=]() { emit receivedResponse(prevResp); });
    }
  }
}

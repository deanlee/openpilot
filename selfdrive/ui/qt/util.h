#pragma once

#include <QDateTime>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmapCache>
#include <QSurfaceFormat>
#include <QWidget>

QString getBrand();
QString getBrandVersion();
void configFont(QPainter &p, const QString &family, int size, const QString &style);
void clearLayout(QLayout* layout);
void setQtSurfaceFormat();
QString timeAgo(const QDateTime &date);
void swagLogMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
void initApp();

class ClickableWidget : public QWidget
{
  Q_OBJECT

public:
  ClickableWidget(QWidget *parent = nullptr);

protected:
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *) override;

signals:
  void clicked();
};

template<class Functor>
void cachePaint(const QString &key, QPainter &painter, QRect &rc, Functor functor) {
  QPixmap pm;
  if (!QPixmapCache::find(key, &pm)) {
    QPixmap tmpPm(rc.width(), rc.height());
    tmpPm.fill(Qt::transparent);
    QPainter p(&tmpPm);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(-rc.left(), -rc.top());
    functor(p, rc);
    pm = tmpPm;
    QPixmapCache::insert(key, pm);
  }
  painter.drawPixmap(rc.left(), rc.top(), pm);
}

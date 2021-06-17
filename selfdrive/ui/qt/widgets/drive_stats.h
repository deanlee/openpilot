#pragma once

#include <QJsonDocument>
#include <QStandardItemModel>
#include <QWidget>
class DriveStats : public QWidget {
  Q_OBJECT

public:
  explicit DriveStats(QWidget* parent = 0);

private:
  void showEvent(QShowEvent *event) override;
  void updateStats();
  inline QString getDistanceUnit() const { return metric_ ? "KM" : "MILES"; }

  bool metric_;
  QJsonDocument stats_;
  QStandardItemModel *model;

private slots:
  void parseResponse(const QString &response);
};

#include "selfdrive/ui/qt/widgets/drive_stats.h"

#include <QDataWidgetMapper>
#include <QDebug>
#include <QGridLayout>
#include <QJsonObject>
#include <QLabel>

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/request_repeater.h"

const double MILE_TO_KM = 1.60934;

namespace {

QLabel* newLabel(const QString& text, bool unitLabel = true, QDataWidgetMapper* mapper = nullptr) {
  QLabel* label = new QLabel(text);
  label->setStyleSheet(unitLabel ? "font-size: 45px; font-weight: 500;" : "font-size: 80px; font-weight: 600;");
  if (mapper) mapper->addMapping(label, mapper->currentIndex());
  return label;
}

}  // namespace

DriveStats::DriveStats(QWidget* parent) : QWidget(parent) {
  metric_ = Params().getBool("IsMetric");
  QGridLayout* main_layout = new QGridLayout(this);

  QDataWidgetMapper* mapper = new QDataWidgetMapper(this);
  model = new QStandardItemModel(0, 8, this);
  mapper->setModel(model);
  for (auto time : {"ALL TIME", "PAST WEEK"}) {
    int row = main_layout->rowCount();

    QLabel* title = new QLabel(time);
    main_layout->addWidget(title, row++, 0, 1, 3);
    title->setStyleSheet("font-size: 48px; font-weight: 500;");

    main_layout->addWidget(newLabel("0", false, mapper), row, 0, Qt::AlignLeft);
    main_layout->addWidget(newLabel("0", false, mapper), row, 1, Qt::AlignLeft);
    main_layout->addWidget(newLabel("0", false, mapper), row, 2, Qt::AlignLeft);

    main_layout->addWidget(newLabel("DRIVES"), row + 1, 0, Qt::AlignLeft);
    main_layout->addWidget(newLabel(getDistanceUnit(), true, mapper), row + 1, 1, Qt::AlignLeft);
    main_layout->addWidget(newLabel("HOURS"), row + 1, 2, Qt::AlignLeft);
  }
  mapper->toFirst();

  std::string dongle_id = Params().get("DongleId");
  if (util::is_valid_dongle_id(dongle_id)) {
    std::string url = "https://api.commadotai.com/v1.1/devices/" + dongle_id + "/stats";
    RequestRepeater* repeater = new RequestRepeater(this, QString::fromStdString(url), "ApiCache_DriveStats", 30);
    QObject::connect(repeater, &RequestRepeater::receivedResponse, this, &DriveStats::parseResponse);
  }
  updateStats();
}

void DriveStats::updateStats() {
  QJsonObject json = stats_.object();
  QJsonObject a[] = {json["all"].toObject(), json["week"].toObject()};
  for (int i = 0; i < 2; ++i) {
    auto& j = a[i];
    model->setData(model->index(0, i * 2), (int)(j["routes"].toDouble()));
    model->setData(model->index(0, i * 2 + 1), (int)j["distance"].toDouble() * (metric_ ? MILE_TO_KM : 1));
    model->setData(model->index(0, i * 2 + 2), (int)(j["minutes"].toDouble() / 60));
    model->setData(model->index(0, i * 2 + 3), getDistanceUnit());
  }
}

void DriveStats::parseResponse(const QString& response) {
  QJsonDocument doc = QJsonDocument::fromJson(response.trimmed().toUtf8());
  if (doc.isNull()) {
    qDebug() << "JSON Parse failed on getting past drives statistics";
    return;
  }
  if (stats_ != doc) {
    stats_ = doc;
    updateStats();
  }
}

void DriveStats::showEvent(QShowEvent* event) {
  bool metric = Params().getBool("IsMetric");
  if (metric_ != metric) {
    metric_ = metric;
    updateStats();
  }
}

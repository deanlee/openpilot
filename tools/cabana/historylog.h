#pragma once

#include <array>
#include <deque>
#include <map>
#include <vector>

#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableView>

#include "tools/cabana/dbc/dbcmanager.h"
#include "tools/cabana/streams/abstractstream.h"
#include "tools/cabana/util.h"

class HeaderView : public QHeaderView {
public:
  HeaderView(Qt::Orientation orientation, QWidget *parent = nullptr) : QHeaderView(orientation, parent) {}
  QSize sectionSizeFromContents(int logicalIndex) const override;
  void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const;
};

class HistoryLogModel : public QAbstractTableModel {
  Q_OBJECT

public:
  HistoryLogModel(QObject *parent) : QAbstractTableModel(parent) {}
  void setMessage(const MessageId &message_id);
  void updateState();
  void setFilter(int sig_idx, const QString &value, std::function<bool(double, double)> cmp);
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  void fetchMore(const QModelIndex &parent) override;
  inline bool canFetchMore(const QModelIndex &parent) const override { return has_more_data; }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return messages.size(); }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override {
    return display_signals_mode && !sigs.empty() ? sigs.size() + 1 : 2;
  }
  void refresh(bool fetch_message = true);

public slots:
  void setDisplayType(int type);
  void segmentsMerged();

public:

   struct MsgSignal {
    MessageId msg_id;
    const cabana::Signal *sig;
  };

  struct Message {
    QVector<double> sig_values;
    QByteArray data;
    QVector<QColor> colors;
    std::array<double, 20> vals;
  };

  //todo use deque
  std::map<uint64_t, Message> messages;

  void fetchData(uint64_t from_time, uint64_t min_time = 0);

  MessageId msg_id;
  CanData hex_colors;
  bool has_more_data = true;
  const int batch_size = 50;
  int filter_sig_idx = -1;
  double filter_value = 0;
  uint64_t last_fetch_time = 0;
  std::function<bool(double, double)> filter_cmp = nullptr;

  std::vector<MsgSignal> sigs;
  bool display_signals_mode = true;
};

class LogsWidget : public QFrame {
  Q_OBJECT

public:
  LogsWidget(QWidget *parent);
  void setMessage(const MessageId &message_id);
  void updateState();
  void showEvent(QShowEvent *event) override;

private slots:
  void setFilter();

private:
  void refresh();

  QTableView *logs;
  HistoryLogModel *model;
  QComboBox *signals_cb, *comp_box, *display_type_cb;
  QLineEdit *value_edit;
  QWidget *filters_widget;
  MessageBytesDelegate *delegate;
};

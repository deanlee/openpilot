#pragma once

#include <deque>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLineEdit>
#include <QTableView>

#include "tools/cabana/dbc/dbcmanager.h"
#include "tools/cabana/streams/abstractstream.h"
#include "tools/cabana/utils/util.h"

class HeaderView : public QHeaderView {
public:
  HeaderView(Qt::Orientation orientation, QWidget *parent = nullptr) : QHeaderView(orientation, parent) {}
  QSize sectionSizeFromContents(int logicalIndex) const override;
  void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const;
};

class AbstractLogModel : public QAbstractTableModel {
  Q_OBJECT
public:
  AbstractLogModel(QObject *parent) : QAbstractTableModel(parent) {}
  void updateState();
  void fetchMore(const QModelIndex &parent) override;
  inline bool canFetchMore(const QModelIndex &parent) const override { return has_more_data; }
  virtual void refresh(bool fetch_message = true) = 0;
  virtual int fetchData(uint64_t from_time, uint64_t min_time = 0) = 0;
  virtual void setMessage(const MessageId &message_id) = 0;
  const int batch_size = 50;
  bool has_more_data = true;
  uint64_t last_fetch_time = 0;
};

class HexLogModel : public AbstractLogModel {
  Q_OBJECT

public:
  HexLogModel(QObject *parent) : AbstractLogModel(parent) {}
  void setMessage(const MessageId &msg_id) override { message_id = msg_id; }
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return messages.size(); }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 2; }
  void refresh(bool fetch_message = true) override;

public:
  struct Message {
    uint64_t mono_time = 0;
    std::vector<uint8_t> data;
    std::vector<QColor> colors;
  };

  MessageId message_id;
  int fetchData(uint64_t from_time, uint64_t min_time) override;

  CanData hex_colors;
  std::deque<Message> messages;
};

class SignalLogModel : public AbstractLogModel {
  Q_OBJECT

public:
  SignalLogModel(QObject *parent) : AbstractLogModel(parent) {}
  void setMessage(const MessageId &message_id) override;
  void addSignal(const MessageId &message_id, const cabana::Signal *signal);
  void setFilter(int sig_idx, const QString &value, std::function<bool(double, double)> cmp);
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex &parent = QModelIndex()) const override { return messages.size(); }
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return sigs.size() + 1; }
  void refresh(bool fetch_message = true) override;

public:
  struct Message {
    uint64_t mono_time = 0;
    std::vector<std::optional<double>> sig_values;
  };
  struct Sig {
    MessageId msg_id;
    const cabana::Signal *sig;
  };

  int fetchData(uint64_t from_time, uint64_t min_time = 0) override;

  int filter_sig_idx = -1;
  double filter_value = 0;
  std::function<bool(double, double)> filter_cmp = nullptr;
  std::deque<Message> messages;
  std::vector<Sig> sigs;
};

class LogsWidget : public QFrame {
  Q_OBJECT

public:
  LogsWidget(QWidget *parent);
  void addSignal(const MessageId &message_id, const cabana::Signal *signal);
  void setMessage(const MessageId &message_id);
  void updateState();
  void showEvent(QShowEvent *event) override;

private slots:
  void setFilter();
  void exportToCSV();

private:
  void refresh();

  QTableView *logs;
  AbstractLogModel *model;
  QComboBox *signals_cb, *comp_box, *display_type_cb;
  QLineEdit *value_edit;
  QWidget *filters_widget;
  MessageId msg_id;
  MessageBytesDelegate *delegate;
};

#pragma once

#include <QAbstractTableModel>
#include <QLabel>
#include <QPushButton>
#include <QTreeView>

#include "tools/cabana/commands.h"
#include "tools/cabana/settings.h"

class FindSignalModel : public QAbstractTableModel {
public:
  struct SearchSignal {
    MessageId id = {};
    uint64_t mono_time = 0;
    cabana::Signal sig = {};
    double value = 0.;
    QStringList values;
  };

  struct Item {
    ~Item() {
      qDeleteAll(cihldren);
    }
    MessageId id = {};
    uint64_t mono_time = 0;
    cabana::Signal sig = {};
    double value = 0.;
    QStringList values;

    Item *parent = nullptr;
    QVector<Item*> children;
  };

  FindSignalModel(QObject *parent) : QAbstractTableModel(parent) {}
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
  QModelIndex parent(const QModelIndex &index) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override { return 2; }
  Qt::ItemFlags flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  void search(std::function<bool(double)> cmp);
  void reset();
  void undo();

  Item filtered_signals;
  Item initial_signals;
  QList<Item> histories;
  uint64_t last_time = std::numeric_limits<uint64_t>::max();
};

class FindSignalDlg : public QDialog {
  Q_OBJECT
public:
  FindSignalDlg(QWidget *parent);

signals:
  void openMessage(const MessageId &id);

private:
  void search();
  void modelReset();
  void setInitialSignals();
  void customMenuRequested(const QPoint &pos);

  QLineEdit *value1, *value2, *factor_edit, *offset_edit;
  QLineEdit *bus_edit, *address_edit, *first_time_edit, *last_time_edit;
  QComboBox *compare_cb;
  QSpinBox *min_size, *max_size;
  QCheckBox *litter_endian, *is_signed;
  QPushButton *search_btn, *reset_btn, *undo_btn;
  QGroupBox *properties_group, *message_group;
  QTreeView *view;
  QLabel *to_label, *stats_label;
  FindSignalModel *model;
};

#include "tools/cabana/historylog.h"

#include <functional>

#include <QDebug>
#include <QFileDialog>
#include <QPainter>
#include <QVBoxLayout>

#include "tools/cabana/commands.h"
#include "tools/cabana/utils/export.h"

void AbstractLogModel::updateState() {
  uint64_t current_time = (can->currentSec() + can->routeStartTime()) * 1e9 + 1;
  int rows = fetchData(current_time, last_fetch_time);
  if (rows > 0) {
    beginInsertRows({}, 0, rows - 1);
    endInsertRows();
  }
  has_more_data = rows >= batch_size;
  last_fetch_time = current_time;
}

void AbstractLogModel::fetchMore(const QModelIndex &parent) {
  int old_rows = rowCount({});
  int rows = fetchData(messages.back().mono_time);
  if (rows > 0) {
    beginInsertRows({}, old_rows, old_rows + rows - 1);
    endInsertRows();
  }
  has_more_data = rows >= batch_size;
}


// HexLogModel

QVariant HexLogModel::data(const QModelIndex &index, int role) const {
  const auto &m = messages[index.row()];
  if (role == Qt::DisplayRole && index.column() == 0) {
    return QString::number((m.mono_time / (double)1e9) - can->routeStartTime(), 'f', 3);
  } else if (role == ColorsRole) {
    return QVariant::fromValue((void *)(&m.colors));
  } else if (role == BytesRole) {
    return QVariant::fromValue((void *)(&m.data));
  } else if (role == Qt::TextAlignmentRole) {
    return (uint32_t)(Qt::AlignRight | Qt::AlignVCenter);
  }
  return {};
}

QVariant HexLogModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
    return (section == 0) ? tr("Time") : tr("Data");
  }
  return {};
}

void HexLogModel::refresh(bool fetch_message) {
  beginResetModel();
  last_fetch_time = 0;
  has_more_data = true;
  messages.clear();
  hex_colors = {};
  if (fetch_message) {
    updateState();
  }
  endResetModel();
}

int HexLogModel::fetchData(uint64_t from_time, uint64_t min_time) {
  std::vector<Message> msgs;
  const auto &events = can->events(message_id);
  auto first = std::upper_bound(events.rbegin(), events.rend(), from_time, [](uint64_t ts, auto e) {
    return ts > e->mono_time;
  });

  for (; first != events.rend() && (*first)->mono_time > min_time; ++first) {
    const CanEvent *e = *first;
    Message &m = msgs.emplace_back();
    m.mono_time = e->mono_time;
    m.data.assign(e->dat, e->dat + e->size);
    if (min_time == 0 && msgs.size() >= batch_size) {
      break;
    }
  }

  const auto freq = can->lastMessage(message_id).freq;
  const std::vector<uint8_t> no_mask;
  const auto speed = can->getSpeed();
  for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
    hex_colors.compute(message_id, it->data.data(), it->data.size(), it->mono_time / (double)1e9, speed, no_mask, freq);
    it->colors = hex_colors.colors;
  }
  messages.insert(min_time > 0 ? msgs.begin() : msgs.end(), std::move_iterator(msgs.begin()), std::move_iterator(msgs.end()));
  return msgs.size();
}

//
QVariant SignalLogModel::data(const QModelIndex &index, int role) const {
  if (role == Qt::DisplayRole) {
    const auto &m = messages[index.row()];
    if (index.column() == 0) {
      return QString::number((m.mono_time / (double)1e9) - can->routeStartTime(), 'f', 3);
    }
    int i = index.column() - 1;
    return m.sig_values[i] ? QString::number(*m.sig_values[i], 'f', sigs[i].sig->precision) : "--";
  } else if (role == Qt::TextAlignmentRole) {
    return (uint32_t)(Qt::AlignRight | Qt::AlignVCenter);
  }
  return {};
}

void SignalLogModel::addSignal(const MessageId &message_id, const cabana::Signal *signal) {
  sigs.push_back({message_id, signal});
}

void SignalLogModel::setMessage(const MessageId &message_id) {
  // msg_id = message_id;
  sigs.clear();
  if (auto dbc_msg = dbc()->msg(message_id)) {
    for (const auto s : dbc_msg->getSignals()) {
      sigs.push_back({message_id, s});
    }
  }
  // refresh(false);
}

void SignalLogModel::refresh(bool fetch_message) {
  beginResetModel();
  last_fetch_time = 0;
  has_more_data = true;
  messages.clear();
  if (fetch_message) {
    updateState();
  }
  endResetModel();
}

QVariant SignalLogModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Horizontal) {
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
      if (section == 0) {
        return "Time";
      }
      QString name = sigs[section - 1].sig->name;
      if (!sigs[section - 1].sig->unit.isEmpty()) {
        name += QString(" (%1)").arg(sigs[section - 1].sig->unit);
      }
      return name;
    } else if (role == Qt::BackgroundRole && section > 0) {
      // Alpha-blend the signal color with the background to ensure contrast
      QColor sigColor = sigs[section - 1].sig->color;
      sigColor.setAlpha(128);
      return QBrush(sigColor);
    }
  }
  return {};
}

void SignalLogModel::setFilter(int sig_idx, const QString &value, std::function<bool(double, double)> cmp) {
  filter_sig_idx = sig_idx;
  filter_value = value.toDouble();
  filter_cmp = value.isEmpty() ? nullptr : cmp;
}

int SignalLogModel::fetchData(uint64_t from_time, uint64_t min_time) {
  std::vector<Message> msgs;
  int i = 0;
  for (const auto &sig : sigs) {
    const auto &events = can->events(sig.msg_id);
    auto first = std::upper_bound(events.rbegin(), events.rend(), from_time, [](uint64_t ts, auto e) {
      return ts > e->mono_time;
    });

    for (; first != events.rend() && (*first)->mono_time > min_time; ++first) {
      const CanEvent *e = *first;
      double value = 0;
      sig.sig->getValue(e->dat, e->size, &value);
      if (!filter_cmp) {// || filter_cmp(values[filter_sig_idx], filter_value)) {
        auto it = std::lower_bound(msgs.begin(), msgs.end(), e->mono_time, [&](auto &m, uint64_t ts) {
          return m.mono_time > ts;
        });
        if (it != msgs.end() && it->mono_time == e->mono_time) {
            it->sig_values[i] = value;
        } else {
          if (msgs.size() >= batch_size && min_time == 0) {
            break;
          }
          Message m;
          m.mono_time = e->mono_time;
          m.sig_values.resize(sigs.size());
          m.sig_values[i] = value;
          msgs.insert(it, m);
        }
      }
    }
    ++i;
  }
  messages.insert(min_time > 0 ? msgs.begin() : msgs.end(), std::move_iterator(msgs.begin()), std::move_iterator(msgs.end()));
  return msgs.size();
}

// HeaderView

QSize HeaderView::sectionSizeFromContents(int logicalIndex) const {
  static const QSize time_col_size = fontMetrics().boundingRect({0, 0, 200, 200}, defaultAlignment(), "000000.000").size() + QSize(10, 6);
  if (logicalIndex == 0) {
    return time_col_size;
  } else {
    int default_size = qMax(100, (rect().width() - time_col_size.width()) / (model()->columnCount() - 1));
    QString text = model()->headerData(logicalIndex, this->orientation(), Qt::DisplayRole).toString();
    const QRect rect = fontMetrics().boundingRect({0, 0, default_size, 2000}, defaultAlignment(), text.replace(QChar('_'), ' '));
    QSize size = rect.size() + QSize{10, 6};
    return QSize{qMax(size.width(), default_size), size.height()};
  }
}

void HeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
  auto bg_role = model()->headerData(logicalIndex, Qt::Horizontal, Qt::BackgroundRole);
  if (bg_role.isValid()) {
    painter->fillRect(rect, bg_role.value<QBrush>());
  }
  QString text = model()->headerData(logicalIndex, Qt::Horizontal, Qt::DisplayRole).toString();
  painter->setPen(palette().color(settings.theme == DARK_THEME ? QPalette::BrightText : QPalette::Text));
  painter->drawText(rect.adjusted(5, 3, -5, -3), defaultAlignment(), text.replace(QChar('_'), ' '));
}

// LogsWidget

LogsWidget::LogsWidget(QWidget *parent) : QFrame(parent) {
  setFrameStyle(QFrame::StyledPanel | QFrame::Plain);
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  QWidget *toolbar = new QWidget(this);
  toolbar->setAutoFillBackground(true);
  QHBoxLayout *h = new QHBoxLayout(toolbar);

  filters_widget = new QWidget(this);
  QHBoxLayout *filter_layout = new QHBoxLayout(filters_widget);
  filter_layout->setContentsMargins(0, 0, 0, 0);
  filter_layout->addWidget(display_type_cb = new QComboBox(this));
  filter_layout->addWidget(signals_cb = new QComboBox(this));
  filter_layout->addWidget(comp_box = new QComboBox(this));
  filter_layout->addWidget(value_edit = new QLineEdit(this));
  h->addWidget(filters_widget);
  h->addStretch(0);
  ToolButton *export_btn = new ToolButton("filetype-csv", tr("Export to CSV file..."));
  h->addWidget(export_btn, 0, Qt::AlignRight);

  display_type_cb->addItems({"Signal", "Hex"});
  display_type_cb->setToolTip(tr("Display signal value or raw hex value"));
  comp_box->addItems({">", "=", "!=", "<"});
  value_edit->setClearButtonEnabled(true);
  value_edit->setValidator(new DoubleValidator(this));

  main_layout->addWidget(toolbar);
  QFrame *line = new QFrame(this);
  line->setFrameStyle(QFrame::HLine | QFrame::Sunken);
  main_layout->addWidget(line);
  main_layout->addWidget(logs = new QTableView(this));
  logs->setModel(model = new SignalLogModel(this));
  delegate = new MessageBytesDelegate(this);
  logs->setHorizontalHeader(new HeaderView(Qt::Horizontal, this));
  logs->horizontalHeader()->setDefaultAlignment(Qt::AlignRight | (Qt::Alignment)Qt::TextWordWrap);
  logs->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  logs->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  logs->verticalHeader()->setDefaultSectionSize(delegate->sizeForBytes(8).height());
  logs->verticalHeader()->setVisible(false);
  logs->setFrameShape(QFrame::NoFrame);

  QObject::connect(display_type_cb, qOverload<int>(&QComboBox::activated), [](int index) {
    // logs->setItemDelegateForColumn(1, index == 1 ? delegate : nullptr);
    // model->setDisplayType(index);
    // delete model;
    // logs->setModel(model = new HexLogModel(this);)
  });

  QObject::connect(signals_cb, SIGNAL(activated(int)), this, SLOT(setFilter()));
  QObject::connect(comp_box, SIGNAL(activated(int)), this, SLOT(setFilter()));
  QObject::connect(value_edit, &QLineEdit::textChanged, this, &LogsWidget::setFilter);
  QObject::connect(export_btn, &QToolButton::clicked, this, &LogsWidget::exportToCSV);
  QObject::connect(can, &AbstractStream::seekedTo, model, &AbstractLogModel::refresh);
  QObject::connect(dbc(), &DBCManager::DBCFileChanged, this, &LogsWidget::refresh);
  QObject::connect(UndoStack::instance(), &QUndoStack::indexChanged, this, &LogsWidget::refresh);
}

void LogsWidget::setMessage(const MessageId &message_id) {
  // model->setMessage(message_id);
  msg_id = message_id;
  refresh();
}

void LogsWidget::refresh() {
  bool has_signal = false;
  if (auto dbc_msg = dbc()->msg(msg_id)) {
    has_signal = dbc_msg->sigs.size();
  }
  if (has_signal) {
    signals_cb->clear();
    // for (auto s : model->sigs) {
    //   signals_cb->addItem(s->name);
    // }
    // model->setFilter(0, "", nullptr);
  }
  bool show_signals = has_signal && display_type_cb->currentIndex() == 1;

  delete model;
  model = show_signals ? (AbstractLogModel*)(new SignalLogModel(this)) : (AbstractLogModel*)(new HexLogModel(this));
  logs->setModel(model);
  model->setMessage(msg_id);
  // model->setFilter(0, "", nullptr);
  logs->setItemDelegateForColumn(1, show_signals ? nullptr : delegate);
  model->refresh(isVisible());
  value_edit->clear();
  comp_box->setCurrentIndex(0);
  filters_widget->setVisible(has_signal);
}

void LogsWidget::setFilter() {
  if (value_edit->text().isEmpty() && !value_edit->isModified()) return;

  std::function<bool(double, double)> cmp = nullptr;
  switch (comp_box->currentIndex()) {
    case 0: cmp = std::greater<double>{}; break;
    case 1: cmp = std::equal_to<double>{}; break;
    case 2: cmp = [](double l, double r) { return l != r; }; break; // not equal
    case 3: cmp = std::less<double>{}; break;
  }
  // model->setFilter(signals_cb->currentIndex(), value_edit->text(), cmp);
  model->refresh();
}

void LogsWidget::updateState() {
  if (isVisible()) {
    model->updateState();
  }
}

void LogsWidget::showEvent(QShowEvent *event) {
  if (model->canFetchMore({}) && model->rowCount() == 0) {
    model->refresh();
  }
}

void LogsWidget::exportToCSV() {
  // QString dir = QString("%1/%2_%3.csv").arg(settings.last_dir).arg(can->routeName()).arg(msgName(model->msg_id));
  // QString fn = QFileDialog::getSaveFileName(this, QString("Export %1 to CSV file").arg(msgName(model->msg_id)),
  //                                           dir, tr("csv (*.csv)"));
  // if (!fn.isEmpty()) {
  //   const bool export_signals = model->display_signals_mode && model->sigs.size() > 0;
  //   export_signals ? utils::exportSignalsToCSV(fn, model->msg_id) : utils::exportToCSV(fn, model->msg_id);
  // }
}

void LogsWidget::addSignal(const MessageId &message_id, const cabana::Signal *signal) {
  // model->addSignal(message_id, signal);
}
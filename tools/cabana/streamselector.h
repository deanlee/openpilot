#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTabWidget>

#include "tools/cabana/streams/abstractstream.h"

class StreamSelector : public QDialog {
  Q_OBJECT

public:
  StreamSelector(QWidget *parent = nullptr);
  void addStreamWidget(AbstractOpenStreamWidget *w);
  QString dbcFile() const { return dbc_file->text(); }
  AbstractStream *stream() const { return stream_; }

private:
  QLineEdit *dbc_file;
  QTabWidget *tab;
  AbstractStream *stream_ = nullptr;
};

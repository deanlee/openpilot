#pragma once

#include <QListWidget>

class ControlListWidget : public  QWidget {
  Q_OBJECT

public:
  explicit ControlListWidget(QWidget *parent = nullptr);
  void addWidget(QWidget *w);
  inline void setSpacing(int spacing) { spacing_ = spacing; }
  inline int spacing() const { return spacing_; }
  QListWidget *listWidget() const { return listWidget_; }

 private:
  QListWidget *listWidget_;
  int spacing_ = 20;
  
};

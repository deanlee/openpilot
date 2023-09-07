#pragma once

#include <QToolButton>

class ToolButton : public QToolButton {
  Q_OBJECT

public:
  ToolButton(const QString &icon, const QString &tooltip = {}, QWidget *parent = nullptr);
  void setIcon(const QString &icon);

private:
  void updateIcon();

  QString icon_str;
  int theme;
};

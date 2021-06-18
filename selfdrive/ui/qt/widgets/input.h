#pragma once

#include <QDialog>

#include "selfdrive/ui/qt/widgets/keyboard.h"

class QLineEdit;
class QVBoxLayout;
class QLabel;
class QWidget;

class InputDialog : public QDialog {
  Q_OBJECT

public:
  explicit InputDialog(const QString &prompt_text, QWidget* parent = 0);
  static QString getText(const QString &prompt, int minLength = -1);
  QString text();
  void setMessage(const QString &message, bool clearInputField = true);
  void setMinLength(int length);
  void show();

private:
  int minLength;
  QLineEdit *line;
  Keyboard *k;
  QLabel *label;
  QVBoxLayout *main_layout;

public slots:
  int exec() override;

private slots:
  void handleInput(const QString &s);

signals:
  void cancel();
  void emitText(const QString &text);
};

class ConfirmationDialog : public QDialog {
  Q_OBJECT

public:
  explicit ConfirmationDialog(const QString &prompt_text, const QString &confirm_text = "Ok",
                              const QString &cancel_text = "Cancel", QWidget* parent = 0);
  static bool alert(const QString &prompt_text, QWidget *parent = 0);
  static bool confirm(const QString &prompt_text, QWidget *parent = 0);

private:
  QLabel *prompt;
  QVBoxLayout *main_layout;

public slots:
  int exec() override;
};

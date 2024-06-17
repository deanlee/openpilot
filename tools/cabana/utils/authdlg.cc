#include "tools/cabana/utils/authdlg.h"

#include <QGroupBox>
#include <QRadioButton>
#include <QVBoxLayout>

AuthDialog::AuthDialog(QWidget *parent) : QDialog(parent) {
  QVBoxLayout *layout = new QVBoxLayout (this);
  QGroupBox *groupbox = new QGroupBox("Method");
  layout->addWidget(groupbox);
  QVBoxLayout *method_layout = new QVBoxLayout(groupbox);
  QRadioButton *btn = new QRadioButton(tr("google"), this);
  method_layout->addWidget(btn);
  btn = new QRadioButton(tr("apple"), this);
  method_layout->addWidget(btn);
  btn = new QRadioButton(tr("github"), this);
  method_layout->addWidget(btn);
  btn = new QRadioButton(tr("jwt"), this);
  method_layout->addWidget(btn);
}

#include "selfdrive/ui/qt/widgets/controls.h"

#include <QStyleOption>

AbstractControl::AbstractControl(const QString &title, const QString &desc, const QString &icon, QWidget *parent) : QWidget(parent) {
  QVBoxLayout *main_layout = new QVBoxLayout(this);
  main_layout->setMargin(0);

  hlayout = new QHBoxLayout;
  hlayout->setMargin(0);
  hlayout->setSpacing(20);

  // left icon
  hlayout->addWidget(icon_label = new QLabel(this));
  if (!icon.isEmpty()) {
    icon_label->setPixmap(icon_pixmap = QPixmap(icon).scaledToWidth(80, Qt::SmoothTransformation));
  }
  icon_label->setVisible(!icon.isEmpty());

  // title
  hlayout->addWidget(title_label = new QPushButton(title), 1);
  title_label->setFixedHeight(120);
  title_label->setStyleSheet("font-size: 50px; font-weight: 400; text-align: left; border: none;");

  // value next to control button
  hlayout->addWidget(value = new ElidedLabel());
  value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  value->setStyleSheet("color: #aaaaaa");

  main_layout->addLayout(hlayout);
  // description
  main_layout->addWidget(description = new QLabel(desc));
  description->setStyleSheet("margins:40, 20, 40, 20; font-size: 40px; color: grey");
  description->setWordWrap(true);
  description->setVisible(false);

  main_layout->addStretch(1);
  connect(title_label, &QPushButton::clicked, this, &AbstractControl::titleClicked);
}

void AbstractControl::titleClicked() {
  if (!description->isVisible()) {
    emit showDescriptionEvent();
  }
  if (!description->text().isEmpty()) {
    description->setVisible(!description->isVisible());
  }
}

// controls

ButtonControl::ButtonControl(const QString &title, const QString &text, const QString &desc, QWidget *parent) : AbstractControl(title, desc, "", parent) {
  btn.setText(text);
  btn.setStyleSheet(R"(
    QPushButton {
      padding: 0;
      border-radius: 50px;
      font-size: 35px;
      font-weight: 500;
      color: #E4E4E4;
      background-color: #393939;
    }
    QPushButton:pressed {
      background-color: #4a4a4a;
    }
    QPushButton:disabled {
      color: #33E4E4E4;
    }
  )");
  btn.setFixedSize(250, 100);
  QObject::connect(&btn, &QPushButton::clicked, this, &ButtonControl::clicked);
  hlayout->addWidget(&btn);
}

// ElidedLabel

ElidedLabel::ElidedLabel(QWidget *parent) : ElidedLabel({}, parent) {}

ElidedLabel::ElidedLabel(const QString &text, QWidget *parent) : QLabel(text.trimmed(), parent) {
  setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  setMinimumWidth(1);
}

void ElidedLabel::resizeEvent(QResizeEvent* event) {
  QLabel::resizeEvent(event);
  lastText_ = elidedText_ = "";
}

void ElidedLabel::paintEvent(QPaintEvent *event) {
  const QString curText = text();
  if (curText != lastText_) {
    elidedText_ = fontMetrics().elidedText(curText, Qt::ElideRight, contentsRect().width());
    lastText_ = curText;
  }

  QPainter painter(this);
  drawFrame(&painter);
  QStyleOption opt;
  opt.initFrom(this);
  style()->drawItemText(&painter, contentsRect(), alignment(), opt.palette, isEnabled(), elidedText_, foregroundRole());
}

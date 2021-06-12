#include "selfdrive/ui/qt/widgets/ssh_keys.h"

#include <QHBoxLayout>
#include <QNetworkReply>

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/api.h"
#include "selfdrive/ui/qt/widgets/input.h"

SshControl::SshControl() : AbstractControl("SSH Keys", "Warning: This grants SSH access to all public keys in your GitHub settings. Never enter a GitHub username other than your own. A comma employee will NEVER ask you to add their GitHub username.", "") {
  // setup widget
  hlayout->addStretch(1);

  username_label.setAlignment(Qt::AlignVCenter);
  username_label.setStyleSheet("color: #aaaaaa");
  hlayout->addWidget(&username_label);

  btn.setStyleSheet(R"(
    padding: 0;
    border-radius: 50px;
    font-size: 35px;
    font-weight: 500;
    color: #E4E4E4;
    background-color: #393939;
  )");
  btn.setFixedSize(250, 100);
  QObject::connect(&btn, &QPushButton::released, this, &SshControl::btnClicked);
  hlayout->addWidget(&btn);
  
  username = QString::fromStdString(params.get("GithubUsername"));
  refresh();
}


void SshControl::refresh() {
  username_label.setText(username);
  btn.setText(username.isEmpty() ? "ADD" : "REMOVE");
  btn.setEnabled(true);
}

void SshControl::btnClicked() {
  if (username.isEmpty()) {
    QString uname = InputDialog::getText("Enter your GitHub username");
    if (!uname.isEmpty()) {
      btn.setText("LOADING");
      btn.setEnabled(false);
      getUserKeys(uname);
    }
  } else {
    username = "";
    params.remove("GithubUsername");
    params.remove("GithubSshKeys");
    refresh();
  }
}

void SshControl::getUserKeys(const QString &uname) {
  HttpRequest *request = new HttpRequest(this, "https://github.com/" + uname + ".keys", "", false);

  auto func = [](const QString &resp, bool success) {
    if (success) {
      if (!resp.isEmpty()) {
        username = uname;
        params.put("GithubUsername", uname.toStdString());
        params.put("GithubSshKeys", resp.toStdString());
      } else {
        ConfirmationDialog::alert("Username '" + uname + "' has no keys on GitHub");
      }
    } else {
      ConfirmationDialog::alert(resp);
    }
    refresh();
    request->deleteLater();
  };

  QObject::connect(request, &HttpRequest::receivedResponse, [=](const QString &resp) {
    func(resp, true);
  });
  QObject::connect(request, &HttpRequest::failedResponse, [=] {
    func(QString("Username '%1' doesn't exist on GitHub").arg(uname), false);
  });
  QObject::connect(request, &HttpRequest::timeoutResponse, [=] {
    func("Request timed out", false);
  });
}

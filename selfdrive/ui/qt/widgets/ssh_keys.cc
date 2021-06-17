#include "selfdrive/ui/qt/widgets/ssh_keys.h"

#include "selfdrive/common/params.h"
#include "selfdrive/ui/qt/api.h"
#include "selfdrive/ui/qt/widgets/input.h"

SshControl::SshControl() : ButtonControl("SSH Keys", "", "Warning: This grants SSH access to all public keys in your GitHub settings. Never enter a GitHub username other than your own. A comma employee will NEVER ask you to add their GitHub username.") {
  username_label.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  username_label.setStyleSheet("color: #aaaaaa");
  hlayout->insertWidget(1, &username_label);

  QObject::connect(this, &ButtonControl::released, [=]() {
    if (text() == "ADD") {
      QString username = InputDialog::getText("Enter your GitHub username");
      if (username.length() > 0) {
        setText("LOADING");
        setEnabled(false);
        getUserKeys(username);
      }
    } else {
      params.remove("GithubUsername");
      params.remove("GithubSshKeys");
      refresh();
    }
  });

  refresh();
}

void SshControl::refresh() {
  QString username = QString::fromStdString(params.get("GithubUsername"));
  username_label.setText(username);
  setText(username.isEmpty() ? "ADD" : "REMOVE");
  setEnabled(true);
}

void SshControl::getUserKeys(const QString &username) {
  HttpRequest *request = new HttpRequest(this, "https://github.com/" + username + ".keys", "", false);
  auto handleResponse = [=](const QString &err) {
    refresh();
    request->deleteLater();
    if (!err.isEmpty()) {
      ConfirmationDialog::alert(err);
    }
  };
  QObject::connect(request, &HttpRequest::receivedResponse, [=](const QString &resp) {
    if (!resp.isEmpty()) {
      params.put("GithubUsername", username.toStdString());
      params.put("GithubSshKeys", resp.toStdString());
    }
    QString err = resp.isEmpty() ? QString("Username '%1' has no keys on GitHub").arg(username) : "";
    handleResponse(err);
  });
  QObject::connect(request, &HttpRequest::failedResponse, [=] {
    handleResponse(QString("Username '%1' doesn't exist on GitHub").arg(username));
  });
  QObject::connect(request, &HttpRequest::timeoutResponse, [=] {
    handleResponse("Request timed out");
  });
}

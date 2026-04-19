#include "StoreWindow.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProcess>
#include <QSettings>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStandardPaths>
#include <QUrl>
#include <QWebEngineCertificateError>

StoreWindow::StoreWindow(QWidget *parent)
    : QMainWindow(parent), pendingRequests(0) {
  setWindowTitle("TITAN STORE");
  resize(1024, 768);
  networkManager = new QNetworkAccessManager(this);
  loadLocalManifests();
  webView = new QWebEngineView(this);
  setCentralWidget(webView);
  webPage = new StoreWebPage(this);
  webView->setPage(webPage);
  connect(webPage, &StoreWebPage::nativeActionRequested, this,
          &StoreWindow::handleWebAction);

  connect(webPage, &QWebEnginePage::certificateError,
          [](QWebEngineCertificateError error) { error.acceptCertificate(); });

  webView->load(QUrl("qrc:/index.html"));
  connect(webView, &QWebEngineView::loadFinished, this,
          &StoreWindow::fetchStoreUrls);
}

StoreWindow::~StoreWindow() {}

QStringList StoreWindow::getInstallPaths() {
  QSettings settings;
  QStringList paths =
      settings.value("installPaths", QStringList()).toStringList();
  if (paths.isEmpty()) {
    paths.append(QDir::homePath() + "/.local/share/TITAN_STORE/apps");
  }
  return paths;
}

QString StoreWindow::getPrimaryInstallPath() {
  return getInstallPaths().first();
}

void StoreWindow::fetchStoreUrls() {
  QSettings settings;
  QStringList urls = settings.value("storeUrls", QStringList()).toStringList();
  if (urls.isEmpty()) {
    urls.append(
        "https://tiwut.github.io/TITAN-STORE-Repository/LINUX/default.json");
  }

  QStringList paths = getInstallPaths();

  QJsonObject settingsObj;
  settingsObj["urls"] = QJsonArray::fromStringList(urls);
  settingsObj["installPaths"] = QJsonArray::fromStringList(paths);
  settingsObj["createShortcut"] =
      settings.value("createShortcut", true).toBool();
  settingsObj["theme"] = settings.value("theme", "system").toString();

  QString script =
      QString("loadSettings(%1);")
          .arg(QString(
              QJsonDocument(settingsObj).toJson(QJsonDocument::Compact)));
  webView->page()->runJavaScript(script);

  allStoreApps.clear();

  pendingRequests = urls.size();
  for (QString urlStr : urls) {
    urlStr = urlStr.trimmed().remove('\r').remove('\n');
    if (urlStr.isEmpty()) {
      pendingRequests--;
      continue;
    }
    QNetworkRequest request((QUrl(urlStr)));
    QSslConfiguration config = request.sslConfiguration();
    config.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(config);

    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::sslErrors,
            [reply]() { reply->ignoreSslErrors(); });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onJsonDownloaded(reply); });
  }

  if (pendingRequests <= 0)
    sendDataToHtml();
}

void StoreWindow::onJsonDownloaded(QNetworkReply *reply) {
  pendingRequests--;
  if (reply->error() == QNetworkReply::NoError) {
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isArray()) {
      for (const QJsonValue &value : doc.array()) {
        QJsonObject obj = value.toObject();
        AppManifest app;
        app.id = obj["id"].toString();
        app.title = obj["title"].toString();
        app.version = obj["version"].toString();
        app.description = obj["description"].toString();
        app.downloadUrl = obj["download_url"].toString();
        app.developer = obj["developer"].toString();
        app.copyright = obj["copyright"].toString();
        app.size = obj["size"].toString();
        app.iconUrl = obj["icon"].toString();
        app.executable = obj["executable"].toString();
        if (app.executable.isEmpty()) {
            app.executable = app.id;
        }
        for (const QJsonValue &cat : obj["categories"].toArray()) {
          app.categories.append(cat.toString());
        }
        for (const QJsonValue &perm : obj["permissions"].toArray()) {
          app.permissions.append(perm.toString());
        }
        for (const QJsonValue &screen : obj["screenshots"].toArray()) {
          app.screenshots.append(screen.toString());
        }
        allStoreApps.insert(app.id, app);
      }
    }
  }
  reply->deleteLater();

  if (pendingRequests <= 0) {
    sendDataToHtml();
  }
}

void StoreWindow::sendDataToHtml() {
  QJsonArray appsArray;
  for (const AppManifest &app : allStoreApps) {
    QJsonObject obj;
    obj["id"] = app.id;
    obj["title"] = app.title;
    obj["developer"] = app.developer;
    obj["icon"] = app.iconUrl;
    QJsonArray categories;
    for (const QString &c : app.categories) categories.append(c);
    obj["categories"] = categories;
    QJsonArray permissions;
    for (const QString &p : app.permissions) permissions.append(p);
    obj["permissions"] = permissions;
    QJsonArray screens;
    for (const QString &s : app.screenshots) screens.append(s);
    obj["screenshots"] = screens;
    obj["orphaned"] = false;
    appsArray.append(obj);
  }

  // Include installed apps whose repository was removed ("orphaned").
  // These use the full metadata cached in their local manifest.json.
  for (const AppManifest &app : installedApps) {
    if (!allStoreApps.contains(app.id)) {
      QJsonObject obj;
      obj["id"]          = app.id;
      obj["title"]       = app.title.isEmpty() ? app.id : app.title;
      obj["developer"]   = app.developer.isEmpty() ? tr("(Repository removed)") : app.developer;
      obj["icon"]        = app.iconUrl;
      QJsonArray cats, perms, screens;
      for (const QString &c : app.categories)  cats.append(c);
      for (const QString &p : app.permissions) perms.append(p);
      for (const QString &s : app.screenshots) screens.append(s);
      obj["categories"]  = cats;
      obj["permissions"] = perms;
      obj["screenshots"] = screens;
      obj["orphaned"]    = true;
      appsArray.append(obj);
    }
  }

  QString appsJson = QJsonDocument(appsArray).toJson(QJsonDocument::Compact);

  QJsonArray installedIdsArray;
  for (const QString &id : installedApps.keys())
    installedIdsArray.append(id);
  QString instJson =
      QJsonDocument(installedIdsArray).toJson(QJsonDocument::Compact);

  QString script = QString("renderStore(%1, %2);").arg(appsJson, instJson);
  webView->page()->runJavaScript(script);
}


void StoreWindow::handleWebAction(QString action, QString payload) {
  if (action == "details") {
    AppManifest selectedApp;
    bool isOrphaned = false;
    if (allStoreApps.contains(payload)) {
      selectedApp = allStoreApps.value(payload);
    } else if (installedApps.contains(payload)) {
      // Orphaned app: use full metadata cached in local manifest.json
      selectedApp = installedApps.value(payload);
      if (selectedApp.title.isEmpty()) selectedApp.title = selectedApp.id;
      if (selectedApp.description.isEmpty())
        selectedApp.description = tr("This app is installed on your system but its source repository is no longer active. You can still launch or uninstall it.");
      isOrphaned = true;
    }

    QJsonObject obj;
    obj["id"] = selectedApp.id;
    obj["title"] = selectedApp.title;
    obj["version"] = selectedApp.version;
    obj["description"] = selectedApp.description;
    obj["developer"] = selectedApp.developer;
    obj["copyright"] = selectedApp.copyright;
    obj["size"] = selectedApp.size;
    obj["icon"] = selectedApp.iconUrl;
    obj["orphaned"] = isOrphaned;
    
    QJsonArray categories;
    for (const QString &c : selectedApp.categories) categories.append(c);
    obj["categories"] = categories;
    
    QJsonArray permissions;
    for (const QString &p : selectedApp.permissions) permissions.append(p);
    obj["permissions"] = permissions;
    
    QJsonArray screens;
    for (const QString &s : selectedApp.screenshots)
      screens.append(s);
    obj["screenshots"] = screens;

    QString appJson = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    bool isInst = installedApps.contains(selectedApp.id);

    QString script = QString("showAppDetails(%1, %2);")
                         .arg(appJson, isInst ? "true" : "false");
    webView->page()->runJavaScript(script);

  } else if (action == "install") {
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (doc.isObject()) {
      QJsonObject obj = doc.object();
      installApp(obj["id"].toString(), obj["path"].toString());
    } else {
      installApp(payload, "");
    }
  } else if (action == "uninstall") {
    uninstallApp(payload);
  } else if (action == "launch") {
    launchApp(payload);
  } else if (action == "installdep") {
    installDependency(payload);
  } else if (action == "savesettings") {
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    QJsonObject obj = doc.object();

    QStringList urls = obj["urls"].toString().split("\n", Qt::SkipEmptyParts);
    QStringList paths = obj["paths"].toString().split("\n", Qt::SkipEmptyParts);
    if (paths.isEmpty()) {
      paths.append(QDir::homePath() + "/.local/share/TITAN_STORE/apps");
    }

    QSettings settings;
    settings.setValue("storeUrls", urls);
    settings.setValue("installPaths", paths);
    settings.setValue("createShortcut", obj["createShortcut"].toBool());
    settings.setValue("theme", obj["theme"].toString());

    loadLocalManifests();
    fetchStoreUrls();
  }
}

void StoreWindow::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal,
                                     QString appId) {
  if (bytesTotal > 0) {
    int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
    QString script =
        QString("updateProgress('%1', %2);").arg(appId).arg(percentage);
    webView->page()->runJavaScript(script);
  }
}

void StoreWindow::installApp(QString appId, QString targetPath) {
  if (activeDownloads.contains(appId))
    return;

  AppManifest appToInstall;
  if (allStoreApps.contains(appId)) {
    appToInstall = allStoreApps.value(appId);
  }
  
  if (appToInstall.id.isEmpty() || appToInstall.downloadUrl.isEmpty()) {
    QMessageBox::warning(this, "Error",
                         "No valid Download URL found in manifest.");
    return;
  }

  QString installDir;
  if (!targetPath.isEmpty()) {
    installDir = targetPath + "/" + appId;
    QDir().mkpath(installDir);
  } else {
    installDir = getAppInstallDir(appToInstall.id);
  }

  QNetworkRequest request((QUrl(appToInstall.downloadUrl)));
  QSslConfiguration config = request.sslConfiguration();
  config.setPeerVerifyMode(QSslSocket::VerifyNone);
  request.setSslConfiguration(config);

  QNetworkReply *reply = networkManager->get(request);
  connect(reply, &QNetworkReply::sslErrors,
          [reply]() { reply->ignoreSslErrors(); });

  activeDownloads.insert(appId, reply);

  connect(reply, &QNetworkReply::downloadProgress, this,
          [this, appId](qint64 bytesReceived, qint64 bytesTotal) {
            onDownloadProgress(bytesReceived, bytesTotal, appId);
          });

  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, installDir, appToInstall]() {
        activeDownloads.remove(appToInstall.id);

        if (reply->error() == QNetworkReply::NoError) {
          QString packagePath = installDir + "/package.tar.gz";
          QFile packageFile(packagePath);

          if (packageFile.open(QIODevice::WriteOnly)) {
            packageFile.write(reply->readAll());
            packageFile.close();
            
            QProcess *process = new QProcess(this);
            process->setWorkingDirectory(installDir);

            webView->page()->runJavaScript(
                QString("updateProgress('%1', 'processing');")
                    .arg(appToInstall.id));

            connect(
                process,
                QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, process, appToInstall, installDir, packagePath](int exitCode) {
                  QFile::remove(packagePath);
                  
                  if (exitCode == 0) {
                    saveLocalManifest(appToInstall);

                    QSettings settings;
                    if (settings.value("createShortcut", true).toBool()) {
                      QString desktopPath =
                          QStandardPaths::writableLocation(
                              QStandardPaths::ApplicationsLocation) +
                          "/titanstore-" + appToInstall.id + ".desktop";
                      QFile dFile(desktopPath);
                      if (dFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        QString content =
                            "[Desktop Entry]\nType=Application\nName=" +
                            appToInstall.title + "\nExec=" + installDir + "/" +
                            appToInstall.executable + "\nIcon=" + installDir +
                            "/icon.png\nTerminal=false\nCategories=Game;\n";
                        dFile.write(content.toUtf8());
                        dFile.close();
                      }
                    }

                    webView->page()->runJavaScript(
                        QString("appInstalled('%1');").arg(appToInstall.id));
                    sendDataToHtml();
                  } else {
                    webView->page()->runJavaScript(
                        QString("appInstallFailed('%1', 'Exit Code %2');")
                            .arg(appToInstall.id)
                            .arg(exitCode));
                  }
                  process->deleteLater();
                });

            process->start("tar", QStringList() << "-xzf" << "package.tar.gz");
          }
        } else {
          webView->page()->runJavaScript(
              QString("appInstallFailed('%1', '%2');")
                  .arg(appToInstall.id, reply->errorString()));
        }
        reply->deleteLater();
      });
}

void StoreWindow::uninstallApp(QString appId) {
  if (!installedApps.contains(appId))
    return;

  QString installDir = getAppInstallDir(appId);
  QDir dir(installDir);
  if (dir.exists()) {
    dir.removeRecursively();
  }

  QString desktopPath =
      QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) +
      "/titanstore-" + appId + ".desktop";
  QFile::remove(desktopPath);

  installedApps.remove(appId);
  saveInstalledIndex();
  webView->page()->runJavaScript(QString("appUninstalled('%1');").arg(appId));
  sendDataToHtml();
}

void StoreWindow::launchApp(QString appId) {
  if (!installedApps.contains(appId))
    return;

  AppManifest app = installedApps.value(appId);
  QString installDir = getAppInstallDir(appId);
  QString execPath = installDir + "/" + app.executable;

  QString program;
  QStringList arguments;

  if (app.executable.endsWith(".jar")) {
    program = "java";
    arguments << "-jar" << app.executable;
  } else if (app.executable.endsWith(".py")) {
    program = "python3";
    arguments << app.executable;
  } else if (app.executable.endsWith(".sh")) {
    program = "bash";
    arguments << app.executable;
  } else if (app.executable.endsWith(".AppImage")) {
    program = execPath;
    
    QProcess check;
    check.start("sh", QStringList() << "-c" << "ldconfig -p | grep libfuse.so.2");
    check.waitForFinished();
    if (check.readAll().isEmpty()) {
      webView->page()->runJavaScript(QString("promptMissingDependency('fuse', '%1');").arg(appId));
      return;
    }
  } else {
    program = execPath;
  }

  // Ensure the file is executable
  QFile::setPermissions(execPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                  QFile::ReadGroup | QFile::ExeGroup |
                                  QFile::ReadOther | QFile::ExeOther);

  QProcess::startDetached(program, arguments, installDir);
}

QString StoreWindow::getAppInstallDir(const QString &appId) {
  QStringList paths = getInstallPaths();

  for (const QString &path : paths) {
    QDir dir(path + "/" + appId);
    if (dir.exists() && QFile::exists(path + "/" + appId + "/manifest.json")) {
      return dir.absolutePath();
    }
  }

  QString newPath = getPrimaryInstallPath() + "/" + appId;
  QDir().mkpath(newPath);
  return newPath;
}

void StoreWindow::loadLocalManifests() {
  installedApps.clear();
  QStringList paths = getInstallPaths();

  for (const QString &customPath : paths) {
    QDir dir(customPath);
    if (!dir.exists())
      continue;

    for (const QString &folder :
         dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      QFile file(customPath + "/" + folder + "/manifest.json");
      if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        AppManifest app;
        app.id          = obj["id"].toString();
        app.version     = obj["version"].toString();
        app.executable  = obj["executable"].toString();
        app.title       = obj["title"].toString();
        app.developer   = obj["developer"].toString();
        app.description = obj["description"].toString();
        app.iconUrl     = obj["icon"].toString();
        app.copyright   = obj["copyright"].toString();
        app.size        = obj["size"].toString();
        for (const QJsonValue &c : obj["categories"].toArray())
          app.categories.append(c.toString());
        for (const QJsonValue &p : obj["permissions"].toArray())
          app.permissions.append(p.toString());
        for (const QJsonValue &s : obj["screenshots"].toArray())
          app.screenshots.append(s.toString());
        if (app.executable.isEmpty())
          app.executable = app.id;
        installedApps.insert(app.id, app);
      }
    }
  }

  // After scanning, write/refresh the central installed.json index
  saveInstalledIndex();
}

void StoreWindow::saveLocalManifest(const AppManifest &app) {
  QString installDir = getAppInstallDir(app.id);
  QFile file(installDir + "/manifest.json");
  if (file.open(QIODevice::WriteOnly)) {
    QJsonObject obj;
    // Core fields
    obj["id"]          = app.id;
    obj["version"]     = app.version;
    obj["executable"]  = app.executable;
    // Cached repository metadata for offline/orphaned display
    obj["title"]       = app.title;
    obj["developer"]   = app.developer;
    obj["description"] = app.description;
    obj["icon"]        = app.iconUrl;
    obj["copyright"]   = app.copyright;
    obj["size"]        = app.size;
    QJsonArray cats, perms, screens;
    for (const QString &c : app.categories)  cats.append(c);
    for (const QString &p : app.permissions) perms.append(p);
    for (const QString &s : app.screenshots) screens.append(s);
    obj["categories"]  = cats;
    obj["permissions"] = perms;
    obj["screenshots"] = screens;
    file.write(QJsonDocument(obj).toJson());
  }
  installedApps.insert(app.id, app);
  saveInstalledIndex();
}

void StoreWindow::saveInstalledIndex() {
  // Build the central installed.json at the primary install path root
  QString indexPath = getPrimaryInstallPath() + "/installed.json";
  QDir().mkpath(getPrimaryInstallPath());

  QJsonArray arr;
  for (const AppManifest &app : installedApps) {
    QJsonObject obj;
    obj["id"]         = app.id;
    obj["version"]    = app.version;
    obj["executable"] = app.executable;
    arr.append(obj);
  }

  QFile indexFile(indexPath);
  if (indexFile.open(QIODevice::WriteOnly)) {
    indexFile.write(QJsonDocument(arr).toJson());
  }
}

void StoreWindow::installDependency(QString dep) {
  if (dep == "fuse") {
    bool ok;
    QString text = QInputDialog::getText(this, tr("Authentication Required"),
                                         tr("Root privileges are required to install system dependencies.\nPlease enter your sudo password:"),
                                         QLineEdit::Password,
                                         "", &ok);
    if (ok && !text.isEmpty()) {
      QString pmCmd;
      if (QFile::exists("/usr/bin/apt-get")) {
          pmCmd = "apt-get update && apt-get install -y libfuse2 && ldconfig";
      } else if (QFile::exists("/usr/bin/dnf")) {
          pmCmd = "dnf install -y fuse && ldconfig";
      } else if (QFile::exists("/usr/bin/pacman")) {
          pmCmd = "pacman -Sy --noconfirm fuse2 && ldconfig";
      } else {
          pmCmd = "apt-get install -y libfuse2 && ldconfig"; // Fallback
      }

      QProcess *process = new QProcess(this);
      process->start("sudo", QStringList() << "-S" << "sh" << "-c" << pmCmd);
      process->write((text + "\n").toUtf8());
      process->closeWriteChannel(); // Ensure EOF is sent to sudo

      connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
              [this, process](int exitCode) {
                if (exitCode == 0) {
                  webView->page()->runJavaScript("appInstalled('dep-fuse'); alert('FUSE installed successfully. You can now launch AppImages.');");
                } else {
                  webView->page()->runJavaScript("appInstallFailed('dep-fuse', 'Check password or internet.'); alert('Failed to install FUSE.');");
                }
                process->deleteLater();
              });
    } else {
      webView->page()->runJavaScript("appInstallFailed('dep-fuse', 'Authentication cancelled.');");
    }
  }
}


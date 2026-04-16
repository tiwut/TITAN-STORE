#include "StoreWindow.h"
#include <QSettings>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QUrl>

StoreWindow::StoreWindow(QWidget *parent) : QMainWindow(parent), pendingRequests(0) {
    setWindowTitle("TITAN STORE");
    networkManager = new QNetworkAccessManager(this);
    loadLocalManifests();
    webView = new QWebEngineView(this);
    setCentralWidget(webView);
    webPage = new StoreWebPage(this);
    webView->setPage(webPage);
    connect(webPage, &StoreWebPage::nativeActionRequested, this, &StoreWindow::handleWebAction);
    webView->load(QUrl("qrc:/index.html"));
    connect(webView, &QWebEngineView::loadFinished, this, &StoreWindow::fetchStoreUrls);
}

StoreWindow::~StoreWindow() {}

void StoreWindow::fetchStoreUrls() {
    QSettings settings;
    QStringList urls = settings.value("storeUrls", QStringList()).toStringList();
    QString customPath = settings.value("installPath", "").toString();
    if (customPath.isEmpty()) {
        customPath = QDir::homePath() + "/.local/share/TITAN_STORE/apps";
    }

    QJsonObject settingsObj;
    settingsObj["urls"] = QJsonArray::fromStringList(urls);
    settingsObj["installPath"] = customPath;
    
    QString script = QString("loadSettings(%1);").arg(QString(QJsonDocument(settingsObj).toJson(QJsonDocument::Compact)));
    webView->page()->runJavaScript(script);

    allStoreApps.clear();

    if (urls.isEmpty()) {
        urls.append("https://github.com/tiwut/TITAN-STORE/raw/refs/heads/main/LINUX/APP-JSON/default.json");
    }

    pendingRequests = urls.size();
    for (const QString& urlStr : urls) {
        if(urlStr.trimmed().isEmpty()) { pendingRequests--; continue; }
        QNetworkRequest request((QUrl(urlStr.trimmed())));
        QNetworkReply* reply = networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() { onJsonDownloaded(reply); });
    }
    
    if(pendingRequests <= 0) sendDataToHtml();
}

void StoreWindow::onJsonDownloaded(QNetworkReply* reply) {
    pendingRequests--;
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isArray()) {
            for (const QJsonValue& value : doc.array()) {
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
                for (const QJsonValue& screen : obj["screenshots"].toArray()) {
                    app.screenshots.append(screen.toString());
                }
                allStoreApps.append(app);
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
    for (const AppManifest& app : allStoreApps) {
        QJsonObject obj;
        obj["id"] = app.id;
        obj["title"] = app.title;
        obj["developer"] = app.developer;
        QJsonArray screens;
        for (const QString& s : app.screenshots) screens.append(s);
        obj["screenshots"] = screens;
        appsArray.append(obj);
    }
    
    QString appsJson = QJsonDocument(appsArray).toJson(QJsonDocument::Compact);
    
    QJsonArray installedIdsArray;
    for(const QString& id : installedApps.keys()) installedIdsArray.append(id);
    QString instJson = QJsonDocument(installedIdsArray).toJson(QJsonDocument::Compact);

    QString script = QString("renderStore(%1, %2);").arg(appsJson, instJson);
    webView->page()->runJavaScript(script);
}

void StoreWindow::handleWebAction(QString action, QString payload) {
    if (action == "details") {
        AppManifest selectedApp;
        for(const auto& a : allStoreApps) if(a.id == payload) selectedApp = a;

        QJsonObject obj;
        obj["id"] = selectedApp.id;
        obj["title"] = selectedApp.title;
        obj["version"] = selectedApp.version;
        obj["description"] = selectedApp.description;
        obj["developer"] = selectedApp.developer;
        obj["copyright"] = selectedApp.copyright;
        obj["size"] = selectedApp.size;
        QJsonArray screens;
        for (const QString& s : selectedApp.screenshots) screens.append(s);
        obj["screenshots"] = screens;

        QString appJson = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        bool isInst = installedApps.contains(selectedApp.id);

        QString script = QString("showAppDetails(%1, %2);").arg(appJson, isInst ? "true" : "false");
        webView->page()->runJavaScript(script);
    } 
    else if (action == "install") {
        installApp(payload);
    }
    else if (action == "saveSettings") {
        QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
        QJsonObject obj = doc.object();
        
        QStringList urls = obj["urls"].toString().split("\n", Qt::SkipEmptyParts);
        QString customPath = obj["path"].toString().trimmed();
        
        QSettings settings;
        settings.setValue("storeUrls", urls);
        settings.setValue("installPath", customPath);
        
        loadLocalManifests();
        fetchStoreUrls();
    }
}

void StoreWindow::installApp(QString appId) {
    AppManifest appToInstall;
    for(const auto& a : allStoreApps) if(a.id == appId) appToInstall = a;
    if(appToInstall.id.isEmpty() || appToInstall.downloadUrl.isEmpty()) {
        QMessageBox::warning(this, "Error", "No valid Download URL found in manifest.");
        return;
    }

    QString installDir = getAppInstallDir(appToInstall.id);
    QNetworkRequest request((QUrl(appToInstall.downloadUrl)));
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, installDir, appToInstall]() {
        if (reply->error() == QNetworkReply::NoError) {
            QString scriptPath = installDir + "/install.sh";
            QFile scriptFile(scriptPath);
            
            if (scriptFile.open(QIODevice::WriteOnly)) {
                scriptFile.write(reply->readAll());
                scriptFile.close();
                scriptFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
                QProcess* process = new QProcess(this);
                process->setWorkingDirectory(installDir);
                
                connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, process, appToInstall](int exitCode) {
                    if (exitCode == 0) {
                        saveLocalManifest(appToInstall);
                        QMessageBox::information(this, "Success", appToInstall.title + " has been successfully installed!");
                        sendDataToHtml();
                    } else {
                        QMessageBox::critical(this, "Error", "Installation script failed (Exit Code " + QString::number(exitCode) + ").");
                    }
                    process->deleteLater();
                });

                process->start("bash", QStringList() << scriptPath);
            }
        } else {
            QMessageBox::critical(this, "Download Error", "Failed to download install script: " + reply->errorString());
        }
        reply->deleteLater();
    });
}

QString StoreWindow::getAppInstallDir(const QString& appId) {
    QSettings settings;
    QString customPath = settings.value("installPath", "").toString();
    if (customPath.isEmpty()) {
        customPath = QDir::homePath() + "/.local/share/TITAN_STORE/apps";
    }
    
    QString appPath = customPath + "/" + appId;
    QDir().mkpath(appPath);
    return appPath;
}

void StoreWindow::loadLocalManifests() {
    installedApps.clear();
    QSettings settings;
    QString customPath = settings.value("installPath", "").toString();
    if (customPath.isEmpty()) {
        customPath = QDir::homePath() + "/.local/share/TITAN_STORE/apps";
    }
    
    QDir dir(customPath);
    for (const QString& folder : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile file(customPath + "/" + folder + "/manifest.json");
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();
            AppManifest app;
            app.id = obj["id"].toString();
            app.version = obj["version"].toString();
            installedApps.insert(app.id, app);
        }
    }
}

void StoreWindow::saveLocalManifest(const AppManifest& app) {
    QString installDir = getAppInstallDir(app.id);
    QFile file(installDir + "/manifest.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        obj["id"] = app.id;
        obj["version"] = app.version;
        QJsonDocument doc(obj);
        file.write(doc.toJson());
    }
    installedApps.insert(app.id, app);
}

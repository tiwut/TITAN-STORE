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
    setWindowTitle("100% TITAN STORE");

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
    QString defaultPath = QDir::homePath() + "/Downloads/TITAN_STORE_TEMP";
    QString customPath = settings.value("installPath", defaultPath).toString();

    QJsonObject settingsObj;
    settingsObj["urls"] = QJsonArray::fromStringList(urls);
    settingsObj["installPath"] = customPath;
    
    QString script = QString("loadSettings(%1);").arg(QString(QJsonDocument(settingsObj).toJson(QJsonDocument::Compact)));
    webView->page()->runJavaScript(script);

    allStoreApps.clear();
    if (urls.isEmpty()) {
        urls.append("https://raw.githubusercontent.com/tiwut/TITAN-STORE/refs/heads/main/MAC/APP-JSON/default.json");
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
    if (pendingRequests <= 0) sendDataToHtml();
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
    
    if(appToInstall.downloadUrl.isEmpty()) return;

    QSettings settings;
    QString downloadFolder = settings.value("installPath", QDir::homePath() + "/Downloads/TITAN_STORE_TEMP").toString();
    QDir().mkpath(downloadFolder);
    
    QString dmgPath = downloadFolder + "/" + appToInstall.id + ".dmg";

    QNetworkRequest request((QUrl(appToInstall.downloadUrl)));
    QNetworkReply* reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, dmgPath, appToInstall]() {
        if (reply->error() == QNetworkReply::NoError) {
            QFile file(dmgPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(reply->readAll());
                file.close();

                QProcess* process = new QProcess(this);
                connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), [this, appToInstall](int exitCode) {
                    if (exitCode == 0) {
                        saveLocalManifest(appToInstall);
                        QMessageBox::information(this, "100% TITAN STORE", 
                            appToInstall.title + " has been downloaded and mounted.\n\n"
                            "Please drag the App to your Applications folder.");
                        sendDataToHtml();
                    } else {
                        QMessageBox::critical(this, "Error", "Failed to mount the DMG file.");
                    }
                    process->deleteLater();
                });
                process->start("hdiutil", QStringList() << "attach" << dmgPath);
            }
        } else {
            QMessageBox::critical(this, "Download Error", reply->errorString());
        }
        reply->deleteLater();
    });
}

void StoreWindow::loadLocalManifests() {
    installedApps.clear();
    QSettings settings;
    QString downloadFolder = settings.value("installPath", QDir::homePath() + "/Downloads/TITAN_STORE_TEMP").toString();
    
    QFile file(downloadFolder + "/registry.json");
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject root = doc.object();
        for(auto it = root.begin(); it != root.end(); ++it) {
            AppManifest app;
            app.id = it.key();
            installedApps.insert(app.id, app);
        }
    }
}

void StoreWindow::saveLocalManifest(const AppManifest& app) {
    installedApps.insert(app.id, app);
    QSettings settings;
    QString downloadFolder = settings.value("installPath", QDir::homePath() + "/Downloads/TITAN_STORE_TEMP").toString();
    
    QJsonObject root;
    for(const QString& key : installedApps.keys()) {
        root[key] = "installed";
    }
    
    QFile file(downloadFolder + "/registry.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
}

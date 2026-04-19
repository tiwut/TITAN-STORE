#ifndef STOREWINDOW_H
#define STOREWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QMap>

struct AppManifest {
    QString id;
    QString title;
    QString version;
    QString description;
    QString iconUrl;
    QString downloadUrl;
    QString developer;
    QString copyright;
    QString executable;
    QString size;
    QStringList categories;
    QStringList permissions;
    QStringList screenshots;
};

class StoreWebPage : public QWebEnginePage {
    Q_OBJECT
public:
    explicit StoreWebPage(QObject* parent = nullptr) : QWebEnginePage(parent) {}
signals:
    void nativeActionRequested(QString action, QString payload);
protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override {
        if (url.scheme() == "app") {
            QString action = url.host();
            QString payload;
            
            if (url.hasQuery()) {
                QUrlQuery query(url);
                payload = query.queryItemValue("data", QUrl::FullyDecoded);
            } else {
                payload = url.path().remove(0, 1);
            }

            emit nativeActionRequested(action, payload); 
            return false; 
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class StoreWindow : public QMainWindow {
    Q_OBJECT

public:
    StoreWindow(QWidget *parent = nullptr);
    ~StoreWindow();

private slots:
    void fetchStoreUrls();
    void onJsonDownloaded(QNetworkReply* reply);
    void handleWebAction(QString action, QString payload);
    void sendDataToHtml();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal, QString appId);

private:
    void loadLocalManifests();
    void saveLocalManifest(const AppManifest& app);
    QStringList getInstallPaths();
    QString getPrimaryInstallPath();
    QString getAppInstallDir(const QString& appId);
    void installApp(QString appId, QString targetPath = "");
    void uninstallApp(QString appId);
    
    QWebEngineView* webView;
    StoreWebPage* webPage;
    QNetworkAccessManager* networkManager;
    QList<AppManifest> allStoreApps;
    QMap<QString, AppManifest> installedApps;
    QMap<QString, QNetworkReply*> activeDownloads;
    int pendingRequests;
};

#endif

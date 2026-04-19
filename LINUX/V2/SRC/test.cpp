#include <QWebEnginePage>
#include <QWebEngineCertificateError>
#include <QObject>

class TestClass : public QObject {
    Q_OBJECT
public:
    TestClass() {
        QWebEnginePage page;
        connect(&page, &QWebEnginePage::certificateError, [](QWebEngineCertificateError error) {
            error.ignoreCertificateError();
        });
    }
};

#include "test.moc"

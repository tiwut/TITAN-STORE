#include <QWebEnginePage>
#include <QWebEngineCertificateError>
#include <QObject>
void test_compile() {
    QWebEnginePage page;
    QObject::connect(&page, &QWebEnginePage::certificateError, [](QWebEngineCertificateError error) {
        error.ignoreCertificateError();
    });
}

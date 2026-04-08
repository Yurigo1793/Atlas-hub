#include <QApplication>
#include <QChar>
#include <QIcon>

#ifdef _WIN32
#if defined(__has_include)
#if __has_include(<ShObjIdl_core.h>)
#include <ShObjIdl_core.h>
#define ATLASHUB_HAS_APP_USER_MODEL_ID 1
#elif __has_include(<shobjidl.h>)
#include <shobjidl.h>
#define ATLASHUB_HAS_APP_USER_MODEL_ID 1
#else
#define ATLASHUB_HAS_APP_USER_MODEL_ID 0
#endif
#else
#include <shobjidl.h>
#define ATLASHUB_HAS_APP_USER_MODEL_ID 1
#endif
#endif

#include "core/AppController.h"
#include "utils/Logger.h"

int main(int argc, char *argv[])
{
#if defined(_WIN32) && ATLASHUB_HAS_APP_USER_MODEL_ID
    const HRESULT appIdResult = SetCurrentProcessExplicitAppUserModelID(L"AtlasHub.App");
    if (FAILED(appIdResult)) {
        Logger::instance().warning(QStringLiteral("Failed to set AppUserModelID. HRESULT=0x%1")
                                       .arg(static_cast<qulonglong>(static_cast<unsigned long>(appIdResult)), 8, 16, QChar(u'0')));
    }
#elif defined(_WIN32)
    Logger::instance().warning(QStringLiteral("Windows AppUserModelID API header not found. "
                                              "Taskbar icon grouping may be limited on this toolchain."));
#endif

    QApplication app(argc, argv);
    QApplication::setApplicationName("AtlasHub");
    QApplication::setOrganizationName("AtlasHub");
    app.setWindowIcon(QIcon("app.ico"));

    Logger::instance().info("Application starting");

    AppController controller;
    controller.initialize();

    const int exitCode = app.exec();
    Logger::instance().info("Application shutting down");
    return exitCode;
}

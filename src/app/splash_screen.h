/****************************************************************************
** Argos - splash/wait screen ("all-seeing Argos" artwork)
** SPDX-License-Identifier: BSD-2-Clause
**
** Frameless window displayed while the application is busy on the UI thread
** for a noticeable amount of time: startup initialization and shutdown
** (closing big documents tears down graphics + renders thumbnails, which can
** freeze the main window for several seconds).
****************************************************************************/

#pragma once

#include "../base/signal.h"

#include <QtCore/QString>
#include <QtGui/QPixmap>
#include <QtWidgets/QWidget>

namespace Mayo {

class GuiApplication;

class SplashScreen : public QWidget {
    Q_OBJECT
public:
    // Returns the currently visible splash screen(nullptr if none)
    static SplashScreen* instance();

    // Shows the splash screen(idempotent), centered on the screen hosting 'refWidget'
    static SplashScreen* showScreen(QWidget* refWidget = nullptr);

    // Closes and deletes the splash screen(no-op if none is shown)
    static void closeScreen();

    // Sets the message displayed in the bottom band and repaints immediately: works even
    // while the event loop is blocked(synchronous teardown) or already finished
    void setMessage(const QString& msg);

    // Reports progress of GuiApplication documents teardown through the splash message,
    // ie "문서 정리 중 (n/total)" each time a document is erased
    void trackDocumentsTeardown(GuiApplication* guiApp);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    SplashScreen();
    ~SplashScreen() override;

    QPixmap m_artwork;
    QString m_message;
    int m_dotPhase = 0;
    int m_countDocsTotal = 0;
    int m_countDocsErased = 0;
    SignalConnectionHandle m_sigConnDocErased;
};

} // namespace Mayo

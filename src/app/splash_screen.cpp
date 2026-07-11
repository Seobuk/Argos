/****************************************************************************
** Argos - splash/wait screen ("all-seeing Argos" artwork)
** SPDX-License-Identifier: BSD-2-Clause
****************************************************************************/

#include "splash_screen.h"

#include "../gui/gui_application.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QEventLoop>
#include <QtCore/QPointer>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QIcon>
#include <QtGui/QPainter>
#include <QtGui/QScreen>
#include <QtGui/QWindow>

namespace Mayo {

namespace {

QPointer<SplashScreen> splashInstance;

// Widget size, same aspect ratio as the 640x500 artwork
constexpr QSize splashSize{512, 400};
// Palette of the artwork(Greek vase style)
const QColor splashBlack{13, 11, 8};
const QColor splashOrange{244, 155, 51};
const QColor splashCream{246, 233, 197};

} // namespace

SplashScreen* SplashScreen::instance()
{
    return splashInstance.data();
}

SplashScreen* SplashScreen::showScreen(QWidget* refWidget)
{
    if (!splashInstance)
        splashInstance = new SplashScreen;

    SplashScreen* splash = splashInstance.data();
    QScreen* screen = QGuiApplication::primaryScreen();
    if (refWidget) {
        const QWindow* wnd = refWidget->window()->windowHandle();
        if (wnd && wnd->screen())
            screen = wnd->screen();
    }

    if (screen) {
        const QRect avail = screen->availableGeometry();
        splash->move(avail.center() - QPoint(splash->width() / 2, splash->height() / 2));
    }

    splash->show();
    splash->raise();
    splash->setMessage(splash->m_message); // Force first paint
    return splash;
}

void SplashScreen::closeScreen()
{
    if (splashInstance) {
        splashInstance->close();
        splashInstance->deleteLater();
        splashInstance.clear();
    }
}

SplashScreen::SplashScreen()
    : QWidget(nullptr, Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    // This window must not count towards QApplication "last window closed"(it would prevent
    // application exit when the main window closes while the splash is still visible)
    this->setAttribute(Qt::WA_QuitOnClose, false);
    this->setFixedSize(splashSize);
    // Render at 2x for crispness on high-dpi displays(empty pixmap if the Qt svg icon engine
    // plugin is unavailable, paintEvent() falls back to plain colors in that case)
    m_artwork = QIcon(":/images/splash_argos.svg").pixmap(splashSize * 2);
    m_message = QString("잠시만 기다려 주세요");

    // Animate trailing dots while the event loop is running(liveness hint). During blocked
    // teardown phases the timer can't fire: setMessage() repaints are the fallback
    auto timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, [=]{
        m_dotPhase = (m_dotPhase + 1) % 3;
        this->update();
    });
    timer->start(450);
}

SplashScreen::~SplashScreen()
{
    m_sigConnDocErased.disconnect();
}

void SplashScreen::setMessage(const QString& msg)
{
    m_message = msg;
    this->repaint(); // Synchronous paint: works with a blocked or finished event loop
    // Pump the native queue(user input excluded) so the OS doesn't flag the window
    // as "Not Responding" during long synchronous work on the UI thread
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void SplashScreen::trackDocumentsTeardown(GuiApplication* guiApp)
{
    if (!guiApp || guiApp->guiDocuments().empty())
        return;

    m_countDocsTotal = int(guiApp->guiDocuments().size());
    m_countDocsErased = 0;
    m_sigConnDocErased.disconnect();
    m_sigConnDocErased = guiApp->signalGuiDocumentErased.connectSlot([=](GuiDocument*) {
        ++m_countDocsErased;
        this->setMessage(
            QString("문서 정리 중 (%1/%2)").arg(m_countDocsErased).arg(m_countDocsTotal)
        );
    });
}

void SplashScreen::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::Antialiasing);

    // Frame + orange field, in case the artwork could not be loaded
    painter.fillRect(this->rect(), splashBlack);
    const int frame = qRound(20. * this->width() / 640.);
    const int fieldBottom = qRound(430. * this->height() / 500.);
    painter.fillRect(
        QRect(frame, frame, this->width() - 2 * frame, fieldBottom - frame), splashOrange
    );
    if (!m_artwork.isNull())
        painter.drawPixmap(this->rect(), m_artwork);

    // Message centered in the bottom band of the artwork
    const QRect bandRect(frame, fieldBottom, this->width() - 2 * frame, this->height() - fieldBottom);
    QFont font = this->font();
    font.setPointSizeF(11.5);
    painter.setFont(font);
    painter.setPen(splashCream);
    const QString dots = QString("...").left(m_dotPhase + 1);
    painter.drawText(bandRect, Qt::AlignCenter, m_message + dots);
}

} // namespace Mayo

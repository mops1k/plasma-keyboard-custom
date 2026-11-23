/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include <QtWaylandClient/QWaylandClientExtensionTemplate>
#include <qwayland-ext-data-control-v1.h>

class QSocketNotifier;

/**
 * Puts an image on the desktop clipboard.
 *
 * The keyboard is a Wayland input method, not a regular application: it has no
 * window of its own that the compositor would consider focused, so the ordinary
 * clipboard protocol (wl_data_device) cannot set the selection from here. KWin
 * offers ext_data_control_manager_v1 instead, a clipboard-manager protocol that
 * can set the selection for any MIME type without a focus or a serial.
 *
 * QtWayland implements only the older wlr-data-control flavour of that
 * protocol, so the interface generated from ext-data-control-v1.xml (see
 * src/CMakeLists.txt) is wrapped here by hand. The offer is kept alive until
 * the next image is offered: the application that pastes reads the data from
 * the pipe asynchronously, after the key combination was already sent.
 */
class ClipboardImage : public QWaylandClientExtensionTemplate<ClipboardImage>, public QtWayland::ext_data_control_manager_v1
{
    Q_OBJECT

public:
    explicit ClipboardImage(QObject *parent = nullptr);
    ~ClipboardImage() override;

    /**
     * Offers @p path (a PNG file) as the desktop's clipboard image.
     *
     * Returns false when the compositor does not support data-control, or the
     * file is not a PNG the keyboard would paste. The image stays on the
     * clipboard until the next call.
     */
    bool setImage(const QString &path);

private:
    /**
     * The data source behind the offer. The image is written to the pipe the
     * application reads from asynchronously, so that a large PNG never blocks
     * the keyboard.
     */
    class Source : public QObject, public QtWayland::ext_data_control_source_v1
    {
    public:
        explicit Source(QByteArray data, QObject *parent = nullptr);
        ~Source() override;

    protected:
        void ext_data_control_source_v1_send(const QString &mimeType, int32_t fd) override;
        void ext_data_control_source_v1_cancelled() override;

    private:
        void writeMore();
        void closePipe();

        QByteArray m_data;
        QSocketNotifier *m_notifier = nullptr;
        qsizetype m_offset = 0;
        int m_fd = -1;
    };

    void ensureDataDevice();

    QPointer<Source> m_source;
    struct ::ext_data_control_device_v1 *m_device = nullptr;
};

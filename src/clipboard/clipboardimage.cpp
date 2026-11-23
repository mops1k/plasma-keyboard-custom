/*
    SPDX-FileCopyrightText: 2026 Aleksandr Kvintilyanov <bednyj.mops@gmail.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "clipboardimage.h"

#include <QFile>
#include <QSocketNotifier>

#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandinputdevice_p.h>
#include <QtWaylandClient/private/qwaylandintegration_p.h>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace
{
//! Klipper keeps every copied image in this format, and an application that
//! accepts a pasted image accepts at least this one. The keyboard never
//! transcodes: an image it cannot offer as PNG is not pasted.
constexpr auto imageMimeType = "image/png";
constexpr qint64 maxImageBytes = 32 * 1024 * 1024;

bool isPng(const QByteArray &data)
{
    static const QByteArray signature = QByteArray::fromHex(QByteArrayLiteral("89504e470d0a1a0a"));
    return data.size() > signature.size() && data.startsWith(signature);
}
}

ClipboardImage::ClipboardImage(QObject *parent)
    : QWaylandClientExtensionTemplate<ClipboardImage>(1, parent)
{
}

ClipboardImage::~ClipboardImage()
{
    m_source = nullptr;
    if (m_device) {
        ext_data_control_device_v1_destroy(m_device);
        m_device = nullptr;
    }
    if (isActive()) {
        destroy();
    }
}

void ClipboardImage::ensureDataDevice()
{
    if (m_device || !isActive()) {
        return;
    }
    auto *integration = this->integration();
    if (!integration) {
        return;
    }
    auto *input = integration->display()->defaultInputDevice();
    if (!input) {
        return;
    }
    m_device = get_data_device(input->wl_seat());
}

bool ClipboardImage::setImage(const QString &path)
{
    if (!isActive()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (file.size() <= 0 || file.size() > maxImageBytes) {
        return false;
    }
    QByteArray data = file.readAll();
    if (!isPng(data)) {
        return false;
    }

    ensureDataDevice();
    if (!m_device) {
        return false;
    }

    auto *source = new Source(std::move(data), this);
    source->init(create_data_source());
    source->offer(QLatin1String(imageMimeType));
    if (m_source) {
        // The previous offer is replaced by the one below; its cancellation
        // event finds the object still alive.
        m_source->deleteLater();
    }
    m_source = source;
    ext_data_control_device_v1_set_selection(m_device, source->object());
    return true;
}

ClipboardImage::Source::Source(QByteArray data, QObject *parent)
    : QObject(parent)
    , m_data(std::move(data))
{
}

ClipboardImage::Source::~Source()
{
    closePipe();
}

void ClipboardImage::Source::ext_data_control_source_v1_send(const QString &mimeType, int32_t fd)
{
    if (fd < 0) {
        return;
    }
    if (mimeType != QLatin1String(imageMimeType)) {
        ::close(fd);
        return;
    }
    if (m_fd >= 0) {
        ::close(m_fd);
    }
    m_fd = fd;
    m_offset = 0;
    const int flags = fcntl(m_fd, F_GETFL);
    if (flags >= 0) {
        fcntl(m_fd, F_SETFL, flags | O_NONBLOCK);
    }
    if (!m_notifier) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Write, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &Source::writeMore, Qt::UniqueConnection);
    } else {
        m_notifier->setSocket(m_fd);
    }
    m_notifier->setEnabled(true);
    writeMore();
}

void ClipboardImage::Source::ext_data_control_source_v1_cancelled()
{
    // Another client took over the clipboard selection; this offer is dead.
    closePipe();
    m_data.clear();
    deleteLater();
}

void ClipboardImage::Source::writeMore()
{
    while (m_fd >= 0 && m_offset < m_data.size()) {
        const qint64 written = ::write(m_fd, m_data.constData() + m_offset, size_t(m_data.size() - m_offset));
        if (written > 0) {
            m_offset += written;
            continue;
        }
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        closePipe();
        return;
    }
    closePipe();
}

void ClipboardImage::Source::closePipe()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
    }
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

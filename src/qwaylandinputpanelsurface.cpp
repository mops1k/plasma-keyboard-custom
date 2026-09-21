/*
 * Copyright (c) 2017 Jan Arne Petersen
 * SPDX-FileCopyrightText: 2026 Kristen McWilliam <kristen@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include "inputpanelrole.h"
#include "qwaylandinputpanelsurface_p.h"

#include <QScreen>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcQpaShellIntegration, "qt.qpa.wayland.shell")

QWaylandInputPanelSurface::QWaylandInputPanelSurface(struct ::zwp_input_panel_surface_v1 *object, QtWaylandClient::QWaylandWindow *window)
    : QWaylandShellSurface(window)
    , QtWayland::zwp_input_panel_surface_v1(object)
{
    qCDebug(qLcQpaShellIntegration) << Q_FUNC_INFO;
    window->applyConfigureWhenPossible();
}

QWaylandInputPanelSurface::~QWaylandInputPanelSurface()
{
    qCDebug(qLcQpaShellIntegration) << Q_FUNC_INFO;
    zwp_input_panel_surface_v1_destroy(object());
}

void QWaylandInputPanelSurface::applyConfigure()
{
    const QVariant roleVariant = window()->window()->property("plasmaKeyboardInputPanelRole");
    const int role = roleVariant.isValid() ? roleVariant.toInt() : -1;

    switch (role) {
    case InputPanelRole::OverlayPanel:
        set_overlay_panel();
        break;
    case InputPanelRole::Keyboard:
    default: {
        QtWaylandClient::QWaylandScreen *screen = window()->waylandScreen();
        if (!screen) {
            qCWarning(qLcQpaShellIntegration) << "No Wayland screen available, cannot configure input panel surface";
            return;
        }

        // set_toplevel only tells the compositor which wl_output to anchor this
        // surface to; it does not update QWindow::screen(). Left alone, that
        // stays whatever Qt assigned when the window was created (typically the
        // primary screen), so QML's Screen attached property -- and every panel
        // dimension the style derives from it -- describes the wrong screen on
        // a multi-monitor setup whenever the panel is not shown on the primary
        // one. Assign the actual screen before anchoring so Screen.width/height
        // match where the panel is really going to appear.
        if (QScreen *qScreen = screen->screen()) {
            window()->window()->setScreen(qScreen);
        }

        set_toplevel(screen->output(), position_center_bottom);
        break;
    }
    }

    window()->display()->handleWindowActivated(window());
}

QT_END_NAMESPACE

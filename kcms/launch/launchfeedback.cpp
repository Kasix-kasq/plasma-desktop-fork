/*
 *  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>
 *  Copyright (C) 2017 Eike Hein <hein@kde.org>
 *  Copyright (c) 2019 Cyril Rossi <cyril.rossi@enioka.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 */

#include "launchfeedback.h"

#include <KAboutData>
#include <KLocalizedString>
#include <KPluginFactory>

#include "kwin_interface.h"

#include "launchfeedbacksettings.h"

K_PLUGIN_FACTORY_WITH_JSON(LaunchFactory, "kcm_launchfeedback.json", registerPlugin<LaunchFeedback>();)

LaunchFeedback::LaunchFeedback(QObject *parent, const QVariantList &args)
    : KQuickAddons::ManagedConfigModule(parent, args)
    , m_settings(new LaunchFeedbackSettings(this))
{
    qmlRegisterUncreatableType<LaunchFeedback>("org.kde.private.kcms.launchfeedback", 1, 0, "KCM", QStringLiteral("Cannot create instances of KCM"));
    qmlRegisterType<LaunchFeedbackSettings>();

    KAboutData *about = new KAboutData(QStringLiteral("kcm_launchfeedback"),
        i18n("Configure application launch feedback"),
        QStringLiteral("0.2"), QString(), KAboutLicense::LGPL);
    setAboutData(about);

    setButtons(Apply | Default);
}

LaunchFeedback::~LaunchFeedback()
{
}

LaunchFeedbackSettings *LaunchFeedback::launchFeedbackSettings() const
{
    return m_settings;
}

void LaunchFeedback::save()
{
    ManagedConfigModule::save();

    org::kde::kwin::Effects kwin(QStringLiteral("org.kde.KWin"),
        QStringLiteral("/Effects"),
        QDBusConnection::sessionBus());
    kwin.reconfigureEffect(QStringLiteral("startupfeedback"));
}

#include "launchfeedback.moc"

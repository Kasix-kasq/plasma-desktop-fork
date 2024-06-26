/*
    SPDX-FileCopyrightText: 2018 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kwin_wl_backend.h"

#include <algorithm>

#include <KLocalizedString>
#include <KSharedConfig>

#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QKeySequence>
#include <QStringList>

#include "logging.h"

KWinWaylandBackend::KWinWaylandBackend(QObject *parent)
    : InputBackend(parent)
    , m_deviceManager(new QDBusInterface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/org/kde/KWin/InputDevice"),
                                         QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                         QDBusConnection::sessionBus(),
                                         this))
{
    findDevices();

    m_deviceManager->connection().connect(QStringLiteral("org.kde.KWin"),
                                          QStringLiteral("/org/kde/KWin/InputDevice"),
                                          QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                          QStringLiteral("deviceAdded"),
                                          this,
                                          SLOT(onDeviceAdded(QString)));
    m_deviceManager->connection().connect(QStringLiteral("org.kde.KWin"),
                                          QStringLiteral("/org/kde/KWin/InputDevice"),
                                          QStringLiteral("org.kde.KWin.InputDeviceManager"),
                                          QStringLiteral("deviceRemoved"),
                                          this,
                                          SLOT(onDeviceRemoved(QString)));
}

KWinWaylandBackend::~KWinWaylandBackend()
{
    qDeleteAll(m_devices);
}

void KWinWaylandBackend::findDevices()
{
    QStringList devicesSysNames;
    const QVariant replyDevicesSysNames = m_deviceManager->property("devicesSysNames");
    if (replyDevicesSysNames.isValid()) {
        qCDebug(KCM_MOUSE) << "Devices list received successfully from KWin.";
        devicesSysNames = replyDevicesSysNames.toStringList();
    } else {
        qCCritical(KCM_MOUSE) << "Error on receiving device list from KWin.";
        m_errorString = i18n("Querying input devices failed. Please reopen this settings module.");
        return;
    }

    for (const QString &sn : std::as_const(devicesSysNames)) {
        QDBusInterface deviceIface(QStringLiteral("org.kde.KWin"),
                                   QStringLiteral("/org/kde/KWin/InputDevice/") + sn,
                                   QStringLiteral("org.kde.KWin.InputDevice"),
                                   QDBusConnection::sessionBus(),
                                   this);
        QVariant reply = deviceIface.property("pointer");
        if (reply.isValid() && reply.toBool()) {
            reply = deviceIface.property("touchpad");
            if (reply.isValid() && reply.toBool()) {
                continue;
            }

            KWinWaylandDevice *dev = new KWinWaylandDevice(sn);
            if (!dev->init()) {
                qCCritical(KCM_MOUSE) << "Error on creating device object" << sn;
                m_errorString = i18n("Critical error on reading fundamental device infos of %1.", sn);
                return;
            }
            m_devices.append(dev);
            qCDebug(KCM_MOUSE).nospace() << "Device found: " << dev->name() << " (" << dev->sysName() << ")";
        }
    }
}

bool KWinWaylandBackend::forAllDevices(bool (KWinWaylandDevice::*f)()) const
{
    bool ok = true;
    for (const auto &device : std::as_const(m_devices)) {
        ok &= (device->*f)();
    }
    return ok;
}

KConfigGroup KWinWaylandBackend::mouseButtonRebindsConfigGroup()
{
    return KSharedConfig::openConfig(u"kcminputrc"_s)->group(u"ButtonRebinds"_s).group(u"Mouse"_s);
}

bool KWinWaylandBackend::save()
{
    KConfigGroup buttonGroup = mouseButtonRebindsConfigGroup();

    for (const auto &[buttonName, variant] : std::as_const(m_buttonMapping).asKeyValueRange()) {
        if (const auto keySequence = variant.value<QKeySequence>(); !keySequence.isEmpty()) {
            const auto value = QStringList{u"Key"_s, keySequence.toString(QKeySequence::PortableText)};
            buttonGroup.writeEntry(buttonName, value, KConfig::Notify);
        } else {
            buttonGroup.deleteEntry(buttonName, KConfig::Notify);
        }
    }

    return forAllDevices(&KWinWaylandDevice::save);
}

bool KWinWaylandBackend::load()
{
    m_loadedButtonMapping.clear();
    const KConfigGroup buttonGroup = mouseButtonRebindsConfigGroup();

    for (int i = 1; i <= 24; ++i) {
        const auto buttonName = QLatin1String("ExtraButton%1").arg(QString::number(i));
        const auto entry = buttonGroup.readEntry(buttonName, QStringList());
        if (entry.size() == 2 && entry.first() == QLatin1String("Key")) {
            if (const auto keySequence = QKeySequence::fromString(entry.at(1), QKeySequence::PortableText); !keySequence.isEmpty()) {
                m_loadedButtonMapping.insert(buttonName, keySequence);
            }
        }
    }
    setButtonMapping(m_loadedButtonMapping);

    return forAllDevices(&KWinWaylandDevice::init);
}

bool KWinWaylandBackend::defaults()
{
    return forAllDevices(&KWinWaylandDevice::defaults);
}

bool KWinWaylandBackend::isSaveNeeded() const
{
    return m_buttonMapping != m_loadedButtonMapping || std::ranges::any_of(std::as_const(m_devices), [](KWinWaylandDevice *device) {
               return device->isSaveNeeded();
           });
}

QVariantMap KWinWaylandBackend::buttonMapping() const
{
    return m_buttonMapping;
}

void KWinWaylandBackend::setButtonMapping(const QVariantMap &mapping)
{
    if (m_buttonMapping != mapping) {
        m_buttonMapping = mapping;
        Q_EMIT buttonMappingChanged();
    }
}

void KWinWaylandBackend::onDeviceAdded(QString sysName)
{
    if (std::ranges::any_of(std::as_const(m_devices), [sysName](KWinWaylandDevice *device) {
            return device->sysName() == sysName;
        })) {
        return;
    }

    QDBusInterface deviceIface(QStringLiteral("org.kde.KWin"),
                               QStringLiteral("/org/kde/KWin/InputDevice/") + sysName,
                               QStringLiteral("org.kde.KWin.InputDevice"),
                               QDBusConnection::sessionBus(),
                               this);
    QVariant reply = deviceIface.property("pointer");

    if (reply.isValid() && reply.toBool()) {
        reply = deviceIface.property("touchpad");
        if (reply.isValid() && reply.toBool()) {
            return;
        }

        KWinWaylandDevice *dev = new KWinWaylandDevice(sysName);
        if (!dev->init()) {
            Q_EMIT deviceAdded(false);
            return;
        }

        m_devices.append(dev);
        qCDebug(KCM_MOUSE).nospace() << "Device connected: " << dev->name() << " (" << dev->sysName() << ")";
        Q_EMIT deviceAdded(true);
        Q_EMIT inputDevicesChanged();
    }
}

void KWinWaylandBackend::onDeviceRemoved(QString sysName)
{
    const auto it = std::ranges::find_if(std::as_const(m_devices), [sysName](KWinWaylandDevice *device) {
        return device->sysName() == sysName;
    });
    if (it == m_devices.cend()) {
        return;
    }

    const auto dev = *it;
    qCDebug(KCM_MOUSE).nospace() << "Device disconnected: " << dev->name() << " (" << dev->sysName() << ")";

    int index = std::distance(m_devices.cbegin(), it);
    m_devices.removeAt(index);
    Q_EMIT deviceRemoved(index);
    Q_EMIT inputDevicesChanged();
}

QList<InputDevice *> KWinWaylandBackend::inputDevices() const
{
    QList<InputDevice *> devices;
    devices.reserve(m_devices.count());
    for (const auto &device : m_devices) {
        devices.append(device);
    }
    return devices;
}

int KWinWaylandBackend::deviceCount() const
{
    return m_devices.count();
}

QString KWinWaylandBackend::errorString() const
{
    return m_errorString;
}

#include "moc_kwin_wl_backend.cpp"

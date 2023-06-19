/*
    SPDX-FileCopyrightText: 2020 Niccolò Venerandi <niccolo@venerandi.com>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick 2.4
import QtQuick.Layouts 1.0
import org.kde.plasma.plasmoid 2.0
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.kirigami 2.20 as Kirigami

PlasmoidItem {
    id: root

    readonly property bool isVertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

    Layout.minimumWidth: Plasmoid.containment.corona.editMode && !isVertical ? Kirigami.Units.gridUnit : 1
    Layout.preferredWidth: Layout.minimumWidth
    Layout.maximumWidth:   Layout.minimumWidth

    Layout.minimumHeight: Plasmoid.containment.corona.editMode && isVertical ? Kirigami.Units.gridUnit : Layout.minimumWidth
    Layout.preferredHeight: Layout.minimumHeight
    Layout.maximumHeight: Layout.minimumHeight

    Plasmoid.constraintHints: PlasmaCore.Types.MarginAreasSeparator
    preferredRepresentation: fullRepresentation

    Loader {
        anchors.centerIn: parent
        active: Plasmoid.containment.corona.editMode
        sourceComponent: PlasmaCore.SvgItem {
            height: root.isVertical ? 1 : Math.round(root.height / 2)
            width: root.isVertical ? Math.round(root.width / 2) : 1
            svg: PlasmaCore.Svg {imagePath: "widgets/line"}
            elementId: root.isVertical ? "vertical-line" : "horizontal-line"
        }
    }
}

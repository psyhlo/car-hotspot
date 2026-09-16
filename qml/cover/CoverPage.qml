import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingMedium
        spacing: Theme.paddingMedium

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Hotspot in car")
            font.bold: true
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.highlightColor
        }

        Separator {
            width: parent.width
            color: Theme.secondaryHighlightColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: bluetoothManager.isTargetConnected ? qsTr("Car: Connected") : qsTr("Car: Disconnected")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: bluetoothManager.isTargetConnected ? "#00FF66" : Theme.secondaryColor
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            text: hotspotManager.isHotspotActive ? qsTr("Hotspot: ON") : qsTr("Hotspot: OFF")
            font.pixelSize: Theme.fontSizeExtraSmall
            color: hotspotManager.isHotspotActive ? "#00FF66" : Theme.secondaryColor
        }
    }

    CoverActionList {
        CoverAction {
            iconSource: "image://theme/icon-m-service-wifi"
            onTriggered: appController.toggleHotspotManual(!hotspotManager.isHotspotActive)
        }
    }
}

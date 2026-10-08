import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: toolsPage
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: contentCol.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh Status")
                onClicked: {
                    bluetoothManager.refreshDevices()
                    hotspotManager.checkStatus()
                    systemMonitor.refreshStatus()
                    appController.checkDaemonStatus()
                    appController.reloadSharedLog()
                }
            }
        }

        Column {
            id: contentCol
            width: toolsPage.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Manual Control & Diagnostics")
            }

            SectionHeader {
                text: qsTr("Bluetooth Subsystem Recovery")
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Performs a low-level reset of the Bluetooth daemon, unblocks kernel rfkill state, and re-initializes ConnMan. Use this if the Bluetooth connection hangs or the system toggle gets stuck.")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Restart Bluetooth Subsystem")
                onClicked: {
                    appController.restartBluetooth()
                }
            }

            SectionHeader {
                text: qsTr("Hotspot Manual Control")
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Current State: ") + (hotspotManager.isHotspotActive ? qsTr("Hotspot ACTIVE") : qsTr("Hotspot INACTIVE"))
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: hotspotManager.isHotspotActive ? qsTr("Turn Hotspot OFF") : qsTr("Turn Hotspot ON")
                onClicked: appController.toggleHotspotManual(!hotspotManager.isHotspotActive)
            }

            SectionHeader {
                text: qsTr("System Diagnostics Log")
            }

            Rectangle {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                height: Math.min(Math.max(diagLogText.height + Theme.paddingSmall * 2, Theme.itemSizeMedium), Theme.itemSizeLarge * 4)
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.1)
                radius: Theme.paddingSmall
                clip: true

                SilicaFlickable {
                    anchors.fill: parent
                    anchors.margins: Theme.paddingSmall
                    contentHeight: diagLogText.height
                    clip: true

                    TextArea {
                        id: diagLogText
                        width: parent.width
                        readOnly: true
                        text: appController.logStatus
                        font.pixelSize: Theme.fontSizeTiny
                        color: Theme.secondaryHighlightColor
                    }

                    VerticalScrollDecorator { }
                }
            }
        }
    }
}

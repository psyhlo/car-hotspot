import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: toolsPage
    allowedOrientations: Orientation.All

    // Used only by the screenshot mode (CARHOTSPOT_SHOTDIR in main.cpp).
    function devPrepare(step) {
        if (step === 3) {
            toolsFlickable.contentY = Math.max(0, toolsFlickable.contentHeight - toolsFlickable.height)
        }
        console.log("ToolsPage devPrepare", step, "contentHeight", toolsFlickable.contentHeight,
                    "height", toolsFlickable.height, "contentY", toolsFlickable.contentY)
    }

    SilicaFlickable {
        id: toolsFlickable
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
                enabled: !bluetoothManager.isRestartingBluetooth
                text: bluetoothManager.isRestartingBluetooth
                      ? qsTr("Restarting Bluetooth...")
                      : qsTr("Restart Bluetooth Subsystem")
                onClicked: {
                    appController.restartBluetooth()
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Small
                running: bluetoothManager.isRestartingBluetooth
                visible: running
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: bluetoothManager.isRestartingBluetooth
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Recovering Bluetooth subsystem...")
            }

            // Same notice as on the main page: this build has no background service.
            Column {
                visible: !appController.serviceBuild
                width: toolsPage.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: toolsPage.horizontalCenter
                spacing: Theme.paddingSmall

                Label {
                    width: parent.width
                    text: qsTr("Background automation is available in the OpenRepos version")
                    color: Theme.highlightColor
                    font.pixelSize: Theme.fontSizeSmall
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    text: qsTr("This version monitors the car while the app is running (also in background). The OpenRepos version adds a user service that starts on boot and works after the app is closed.")
                    color: Theme.secondaryColor
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Open OpenRepos page")
                    onClicked: Qt.openUrlExternally(appController.openReposUrl)
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

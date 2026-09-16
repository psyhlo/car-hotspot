import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh Bluetooth devices")
                onClicked: bluetoothManager.refreshDevices()
            }
            MenuItem {
                text: qsTr("Check Hotspot status")
                onClicked: hotspotManager.checkStatus()
            }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Hotspot in car")
            }

            // Status Overview Banner
            Rectangle {
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: statusCol.height + 2 * Theme.paddingMedium
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.rgba(Theme.highlightBackgroundColor, 0.15)
                radius: Theme.paddingSmall

                Column {
                    id: statusCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.paddingMedium
                    spacing: Theme.paddingSmall

                    Row {
                        spacing: Theme.paddingMedium
                        Rectangle {
                            width: Theme.itemSizeExtraSmall / 3
                            height: width
                            radius: width / 2
                            color: bluetoothManager.isTargetConnected ? "#00FF66" : Theme.secondaryColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("Car Bluetooth: ") + (bluetoothManager.isTargetConnected ? qsTr("Connected") : qsTr("Disconnected"))
                            color: Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }

                    Row {
                        spacing: Theme.paddingMedium
                        Rectangle {
                            width: Theme.itemSizeExtraSmall / 3
                            height: width
                            radius: width / 2
                            color: hotspotManager.isHotspotActive ? "#00FF66" : Theme.secondaryColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("Wi-Fi Hotspot: ") + (hotspotManager.isHotspotActive ? qsTr("ACTIVE") : qsTr("INACTIVE"))
                            color: Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("Automation settings")
            }

            TextSwitch {
                text: qsTr("Auto-toggle Hotspot")
                description: qsTr("Start Hotspot when car connects, stop when disconnected")
                checked: appController.autoToggle
                onCheckedChanged: appController.autoToggle = checked
            }

            SectionHeader {
                text: qsTr("Select Car Device")
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: appController.targetAddress !== "" 
                      ? qsTr("Target: %1\n(%2)").arg(appController.targetName !== "" ? appController.targetName : qsTr("Unknown")).arg(appController.targetAddress)
                      : qsTr("No car Bluetooth device selected")
                color: Theme.highlightColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
            }

            Repeater {
                model: bluetoothManager.devices
                delegate: BackgroundItem {
                    id: deviceItem
                    width: column.width
                    height: Theme.itemSizeMedium

                    property bool isSelected: appController.targetAddress === modelData.address

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.rgba(Theme.highlightBackgroundColor, isSelected ? 0.3 : 0.0)
                    }

                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter

                        Row {
                            width: parent.width
                            spacing: Theme.paddingSmall

                            Label {
                                text: modelData.name
                                color: isSelected ? Theme.highlightColor : Theme.primaryColor
                                font.bold: isSelected
                                truncationMode: TruncationMode.Fade
                                width: parent.width - (modelData.connected ? 120 : 0)
                            }

                            Label {
                                visible: modelData.connected
                                text: qsTr("[connected]")
                                color: "#00FF66"
                                font.pixelSize: Theme.fontSizeExtraSmall
                            }
                        }

                        Label {
                            text: modelData.address
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }

                    onClicked: {
                        appController.selectDevice(modelData.address, modelData.name)
                    }
                }
            }

            SectionHeader {
                text: qsTr("Manual Control & Diagnostics")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: hotspotManager.isHotspotActive ? qsTr("Turn Hotspot OFF") : qsTr("Turn Hotspot ON")
                onClicked: appController.toggleHotspotManual(!hotspotManager.isHotspotActive)
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: qsTr("Activity Log:")
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            TextArea {
                width: parent.width
                readOnly: true
                text: appController.logStatus
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryHighlightColor
            }
        }
    }
}

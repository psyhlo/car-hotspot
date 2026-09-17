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
                text: qsTr("Refresh Status & Devices")
                onClicked: {
                    bluetoothManager.refreshDevices()
                    hotspotManager.checkStatus()
                    systemMonitor.refreshStatus()
                    appController.checkDaemonStatus()
                }
            }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Car Hotspot")
                description: qsTr("v0.1.26")
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

                    Row {
                        spacing: Theme.paddingMedium
                        Rectangle {
                            width: Theme.itemSizeExtraSmall / 3
                            height: width
                            radius: width / 2
                            color: systemMonitor.isBatteryCharging ? "#00FF66" : (systemMonitor.batteryChargePercentage <= appController.minBatteryLevel ? "#FF4444" : Theme.highlightColor)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("Battery: %1%%2").arg(systemMonitor.batteryChargePercentage).arg(systemMonitor.isBatteryCharging ? qsTr(" (Charging)") : "")
                            color: Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }

                    Row {
                        spacing: Theme.paddingMedium
                        Rectangle {
                            width: Theme.itemSizeExtraSmall / 3
                            height: width
                            radius: width / 2
                            color: systemMonitor.isRoaming ? "#FF9900" : Theme.secondaryColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("Roaming: ") + (systemMonitor.isRoaming ? qsTr("YES (Active)") : qsTr("No"))
                            color: systemMonitor.isRoaming ? "#FF9900" : Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }

                    Row {
                        spacing: Theme.paddingMedium
                        Rectangle {
                            width: Theme.itemSizeExtraSmall / 3
                            height: width
                            radius: width / 2
                            color: (appController.isDaemonActive || appController.autostartService) ? "#00FF66" : Theme.secondaryColor
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Label {
                            text: qsTr("Background Service: ") + (appController.isDaemonActive ? qsTr("RUNNING (Active)") : (appController.autostartService ? qsTr("Enabled") : qsTr("Stopped")))
                            color: appController.isDaemonActive ? "#00FF66" : Theme.secondaryColor
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("Automation & Safety")
            }

            TextSwitch {
                text: qsTr("Auto-toggle Hotspot")
                description: qsTr("Start Hotspot when car connects, stop when disconnected")
                checked: appController.autoToggle
                onCheckedChanged: appController.autoToggle = checked
            }

            TextSwitch {
                text: qsTr("Run in background & Autostart")
                description: qsTr("Keep monitoring car in background and start automatically on phone reboot")
                checked: appController.autostartService
                onCheckedChanged: appController.autostartService = checked
            }

            TextSwitch {
                text: qsTr("Auto-enable Mobile Data")
                description: qsTr("Ensure cellular data connection is active when starting Hotspot")
                checked: appController.enableCellularAuto
                onCheckedChanged: appController.enableCellularAuto = checked
            }

            TextSwitch {
                text: qsTr("Vibration feedback")
                description: qsTr("Vibrate to confirm when car connects and Hotspot starts")
                checked: appController.vibrateOnConnect
                onCheckedChanged: appController.vibrateOnConnect = checked
            }

            TextSwitch {
                text: qsTr("System notifications")
                description: qsTr("Show banner notification and lockscreen events when car connects/disconnects")
                checked: appController.showNotifications
                onCheckedChanged: appController.showNotifications = checked
            }

            TextSwitch {
                text: qsTr("Block Hotspot in Roaming")
                description: qsTr("Prevent starting Hotspot when abroad/roaming to avoid high cellular data charges")
                checked: appController.blockInRoaming
                onCheckedChanged: appController.blockInRoaming = checked
            }

            Slider {
                width: parent.width
                label: qsTr("Delayed turn off: %1 min").arg(value)
                minimumValue: 0
                maximumValue: 10
                stepSize: 1
                value: appController.stopDelayMinutes
                valueText: value === 0 ? qsTr("Instantly") : qsTr("%1 min").arg(value)
                onValueChanged: {
                    if (appController.stopDelayMinutes !== value) {
                        appController.stopDelayMinutes = value
                    }
                }
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: qsTr("Wait a grace period before stopping Hotspot in case of temporary Bluetooth disconnect.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeTiny
            }

            Slider {
                width: parent.width
                label: qsTr("Minimum battery level: %1%").arg(value)
                minimumValue: 5
                maximumValue: 50
                stepSize: 5
                value: appController.minBatteryLevel
                valueText: value + "%"
                onValueChanged: {
                    if (appController.minBatteryLevel !== value) {
                        appController.minBatteryLevel = value
                    }
                }
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: qsTr("If battery is below this level and phone is not charging, Hotspot will not start (or will automatically turn off) to protect the battery.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeTiny
            }

            SectionHeader {
                text: qsTr("Select Car Bluetooth Device")
            }

            Label {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: appController.targetAddress !== "" 
                      ? qsTr("Selected Car: %1\n(%2)").arg(appController.targetName !== "" ? appController.targetName : qsTr("Car BT")).arg(appController.targetAddress)
                      : qsTr("No car Bluetooth device selected yet. Choose from list below:")
                color: appController.targetAddress !== "" ? Theme.highlightColor : Theme.secondaryColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
            }

            // Manual MAC input option for full flexibility
            Row {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                TextField {
                    id: customMacField
                    width: parent.width - setBtn.width - Theme.paddingMedium
                    placeholderText: qsTr("Or enter MAC (e.g. AA:BB:CC:DD:EE:FF)")
                    label: qsTr("Custom Bluetooth MAC")
                    text: appController.targetAddress
                    EnterKey.onClicked: {
                        if (text.trim().length > 0) {
                            appController.selectDevice(text.trim(), qsTr("Car Bluetooth"))
                        }
                    }
                }

                Button {
                    id: setBtn
                    text: qsTr("Save")
                    anchors.verticalCenter: customMacField.verticalCenter
                    onClicked: {
                        if (customMacField.text.trim().length > 0) {
                            appController.selectDevice(customMacField.text.trim(), qsTr("Car Bluetooth"))
                        }
                    }
                }
            }

            SectionHeader {
                text: qsTr("Paired / Detected Devices (%1)").arg(bluetoothManager.devices.length)
            }

            Label {
                visible: bluetoothManager.devices.length === 0
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Theme.horizontalPageMargin
                anchors.rightMargin: Theme.horizontalPageMargin
                text: qsTr("No paired devices found yet. Pull down to refresh or pair your car in Settings -> Bluetooth.")
                color: Theme.secondaryColor
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
            }

            Repeater {
                model: bluetoothManager.devices
                delegate: ListItem {
                    id: deviceItem
                    contentHeight: Theme.itemSizeMedium

                    property bool isSelected: appController.targetAddress.toUpperCase() === modelData.address.toUpperCase()

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.rgba(Theme.highlightBackgroundColor, isSelected ? 0.35 : 0.0)
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
                                width: parent.width - (modelData.connected ? 140 : (isSelected ? 100 : 0))
                            }

                            Label {
                                visible: isSelected
                                text: qsTr("[CAR]")
                                color: Theme.highlightColor
                                font.bold: true
                                font.pixelSize: Theme.fontSizeExtraSmall
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
                        customMacField.text = modelData.address
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

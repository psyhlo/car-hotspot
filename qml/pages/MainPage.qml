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
                text: qsTr("Manual Control & Diagnostics")
                onClicked: pageStack.push(Qt.resolvedUrl("ToolsPage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh Status & Devices")
                onClicked: {
                    bluetoothManager.refreshDevices()
                    if (!bluetoothManager.isTargetConnected && bluetoothManager.isBluetoothPowered) {
                        bluetoothManager.connectTargetDevices()
                    }
                    hotspotManager.checkStatus()
                    systemMonitor.refreshStatus()
                    appController.checkDaemonStatus()
                    appController.reloadSharedLog()
                }
            }
        }

        Column {
            id: column
            width: page.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Car Hotspot")
                description: (typeof appVersion !== "undefined" && appVersion.length > 0) ? ("v" + appVersion) : ""
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
                            text: qsTr("Car Bluetooth: ") + (bluetoothManager.isTargetConnected ? (bluetoothManager.connectedTargetName !== "" ? bluetoothManager.connectedTargetName : qsTr("Connected")) : qsTr("Disconnected"))
                            color: Theme.primaryColor
                            font.pixelSize: Theme.fontSizeSmall
                            truncationMode: TruncationMode.Fade
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
                text: qsTr("Auto-enable Bluetooth")
                description: qsTr("Turn on Bluetooth automatically if it is powered off")
                checked: appController.autoEnableBluetooth
                onCheckedChanged: appController.autoEnableBluetooth = checked
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

            TextSwitch {
                text: qsTr("Multiple car devices")
                description: qsTr("Allow selecting multiple Bluetooth devices instead of just one")
                checked: appController.allowMultipleDevices
                onCheckedChanged: appController.allowMultipleDevices = checked
            }

            // Selected Cars Visual Cards Section
            Column {
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingSmall

                Row {
                    width: parent.width
                    visible: appController.selectedDevices.length > 0
                    Label {
                        width: parent.width - (clearAllBtn.visible ? clearAllBtn.width : 0)
                        anchors.verticalCenter: parent.verticalCenter
                        text: appController.allowMultipleDevices
                              ? qsTr("Selected Cars (%1)").arg(appController.selectedDevices.length)
                              : qsTr("Selected Car")
                        color: Theme.highlightColor
                        font.pixelSize: Theme.fontSizeSmall
                        font.bold: true
                    }
                    IconButton {
                        id: clearAllBtn
                        visible: appController.allowMultipleDevices && appController.selectedDevices.length > 1
                        icon.source: "image://theme/icon-m-delete"
                        width: Theme.iconSizeMedium
                        height: Theme.iconSizeMedium
                        onClicked: appController.clearDevices()
                    }
                }

                Rectangle {
                    visible: appController.selectedDevices.length === 0
                    width: parent.width
                    height: emptyLabel.height + 2 * Theme.paddingMedium
                    radius: Theme.paddingSmall
                    color: Theme.rgba(Theme.highlightBackgroundColor, 0.1)

                    Label {
                        id: emptyLabel
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: Theme.paddingMedium
                        anchors.verticalCenter: parent.verticalCenter
                        text: appController.allowMultipleDevices
                              ? qsTr("No car Bluetooth devices selected yet. Choose from list below:")
                              : qsTr("No car Bluetooth device selected yet. Choose from list below:")
                        color: Theme.secondaryColor
                        wrapMode: Text.Wrap
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }

                Repeater {
                    model: appController.selectedDevices
                    delegate: Rectangle {
                        width: parent.width
                        height: Theme.itemSizeSmall
                        radius: Theme.paddingSmall
                        color: Theme.rgba(modelData.connected ? "#00FF66" : Theme.highlightBackgroundColor,
                                          modelData.connected ? 0.22 : 0.18)

                        // Left accent indicator bar
                        Rectangle {
                            width: Theme.paddingSmall / 1.5
                            height: parent.height
                            radius: Theme.paddingSmall
                            color: modelData.connected ? "#00FF66" : Theme.highlightColor
                            anchors.left: parent.left
                        }

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.paddingMedium + Theme.paddingSmall
                            anchors.rightMargin: Theme.paddingSmall
                            spacing: Theme.paddingSmall

                            // Live status dot
                            Rectangle {
                                width: Theme.paddingMedium
                                height: width
                                radius: width / 2
                                color: modelData.connected ? "#00FF66" : Theme.highlightColor
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Column {
                                width: parent.width - removeCarBtn.width - Theme.paddingMedium * 3
                                anchors.verticalCenter: parent.verticalCenter

                                Row {
                                    width: parent.width
                                    spacing: Theme.paddingSmall

                                    Label {
                                        text: modelData.name
                                        color: Theme.primaryColor
                                        font.bold: true
                                        font.pixelSize: Theme.fontSizeSmall
                                        truncationMode: TruncationMode.Fade
                                        width: Math.min(implicitWidth, parent.width - (modelData.connected ? 120 : 0))
                                    }

                                    Label {
                                        visible: modelData.connected
                                        text: qsTr("[connected]")
                                        color: "#00FF66"
                                        font.bold: true
                                        font.pixelSize: Theme.fontSizeExtraSmall
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                Label {
                                    text: modelData.address
                                    color: Theme.secondaryColor
                                    font.pixelSize: Theme.fontSizeExtraSmall
                                }
                            }

                            IconButton {
                                id: removeCarBtn
                                icon.source: "image://theme/icon-m-clear"
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: appController.removeDevice(modelData.address)
                            }
                        }
                    }
                }
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

                    // In single-device mode the field mirrors appController.targetAddress
                    // through a live binding; in multi-device mode it shows what the user
                    // typed. Assigning to `text` directly would destroy that binding.
                    property string typedMac: ""
                    text: appController.allowMultipleDevices ? typedMac : appController.targetAddress
                    onTextChanged: if (appController.allowMultipleDevices) typedMac = text

                    // Typing into the field destroys the binding (Qt Quick behaviour),
                    // so re-establish it whenever the multi-device mode is toggled.
                    Connections {
                        target: appController
                        function onAllowMultipleDevicesChanged() {
                            customMacField.text = Qt.binding(function() {
                                return appController.allowMultipleDevices ? customMacField.typedMac
                                                                          : appController.targetAddress
                            })
                        }
                    }

                    EnterKey.onClicked: {
                        if (text.trim().length > 0) {
                            if (appController.allowMultipleDevices) {
                                appController.addDevice(text.trim(), qsTr("Car Bluetooth"))
                                typedMac = ""
                            } else {
                                appController.selectDevice(text.trim(), qsTr("Car Bluetooth"))
                                focus = false
                            }
                        }
                    }
                }

                Button {
                    id: setBtn
                    text: appController.allowMultipleDevices ? qsTr("Add") : qsTr("Save")
                    anchors.verticalCenter: customMacField.verticalCenter
                    onClicked: {
                        if (customMacField.text.trim().length > 0) {
                            if (appController.allowMultipleDevices) {
                                appController.addDevice(customMacField.text.trim(), qsTr("Car Bluetooth"))
                                customMacField.typedMac = ""
                            } else {
                                appController.selectDevice(customMacField.text.trim(), qsTr("Car Bluetooth"))
                                customMacField.focus = false
                            }
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

                    // Directly reactive to both modelData.selected and appController.targetAddresses
                    property bool isSelected: (modelData.selected === true) || (appController.targetAddresses.indexOf(modelData.address.toUpperCase()) !== -1)

                    Rectangle {
                        anchors.fill: parent
                        color: Theme.rgba(Theme.highlightBackgroundColor, isSelected ? 0.30 : 0.0)
                    }

                    Row {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Theme.horizontalPageMargin
                        anchors.rightMargin: Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.paddingMedium

                        // Visual checkbox / selection indicator
                        Rectangle {
                            width: Theme.iconSizeSmall
                            height: Theme.iconSizeSmall
                            radius: appController.allowMultipleDevices ? Theme.paddingSmall / 1.5 : width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            color: isSelected ? Theme.highlightColor : "transparent"
                            border.width: 2
                            border.color: isSelected ? Theme.highlightColor : Theme.secondaryColor

                            Rectangle {
                                visible: isSelected
                                width: parent.width * 0.5
                                height: parent.height * 0.5
                                radius: parent.radius / 2
                                anchors.centerIn: parent
                                color: Theme.primaryColor
                            }
                        }

                        Column {
                            width: parent.width - Theme.iconSizeSmall - Theme.paddingMedium
                            anchors.verticalCenter: parent.verticalCenter

                            Row {
                                width: parent.width
                                spacing: Theme.paddingSmall

                                Label {
                                    text: modelData.name
                                    color: isSelected ? Theme.highlightColor : Theme.primaryColor
                                    font.bold: isSelected
                                    truncationMode: TruncationMode.Fade
                                    width: parent.width - (modelData.connected ? 130 : 0) - (isSelected ? 85 : 0) - Theme.paddingSmall * 2
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
                    }

                    onClicked: {
                        if (appController.allowMultipleDevices) {
                            appController.toggleDeviceSelection(modelData.address, modelData.name)
                        } else {
                            // customMacField.text is bound to appController.targetAddress,
                            // so no imperative assignment here (it would break the binding).
                            appController.selectDevice(modelData.address, modelData.name)
                        }
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
        }
    }
}

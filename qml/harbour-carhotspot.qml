import QtQuick 2.0
import Sailfish.Silica 1.0
import Connman 0.2
import com.jolla.connection 1.0
import "pages"

ApplicationWindow {
    id: appWindow
    initialPage: Component { MainPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Native Sailfish OS ConnMan Wi-Fi Technology & ConnectionAgent
    NetworkTechnology {
        id: wifiTech
        path: Connman.wifiTechnologyPath
        onTetheringChanged: {
            hotspotManager.updateHotspotState(tethering)
        }
        Component.onCompleted: {
            hotspotManager.updateHotspotState(tethering)
        }
    }

    ConnectionAgent {
        id: connAgent
    }

    Connections {
        target: hotspotManager
        onHotspotToggleRequested: {
            if (active) {
                connAgent.startTethering("wifi")
            } else {
                connAgent.stopTethering("wifi", true)
            }
        }
        onRestoreWifiRequested: {
            wifiTech.powered = powered
        }
    }
}

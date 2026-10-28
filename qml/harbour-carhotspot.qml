import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"

ApplicationWindow {
    id: appWindow
    initialPage: Component { MainPage { } }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    // Development aid, paired with the shot mode in src/main.cpp. It only ever runs when
    // shot mode is armed (CARHOTSPOT_SHOTDIR or the devShot* settings keys); the UI never
    // calls it. Steps: 0 = main page as opened, 1 = main page scrolled to the end,
    // 2 = diagnostics page, 3 = diagnostics page scrolled to the end.
    function devShotStep(step) {
        if (step === 2) {
            if (pageStack.depth === 1) {
                pageStack.push(Qt.resolvedUrl("pages/ToolsPage.qml"))
            }
            pageStack.completeAnimation()
            return
        }
        if (pageStack.currentPage && pageStack.currentPage.devPrepare) {
            pageStack.currentPage.devPrepare(step)
        }
        pageStack.completeAnimation()
    }
}

import QtQuick
import QtQuick.Window
import VintageAtlas 1.0

Window {
    id: root
    objectName: "meridianPreviewWindow"
    title: "Meridian — World Clock"
    visible: appWindowVisible
    width: 1600
    height: 700
    color: "#101714"
    property bool captureMode: appCaptureMode
    property bool saverMode: appSaverMode
    property bool wallpaperMode: appWallpaperMode
    property var clockContext: clockModel
    flags: wallpaperMode ? (Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus) : Qt.Window
    MeridianScene {
        id: scene
        anchors.fill: parent
        clockModel: root.clockContext
        captureMode: root.captureMode
        saverMode: root.saverMode
        wallpaperMode: root.wallpaperMode
        onCloseRequested: root.close()
    }
}

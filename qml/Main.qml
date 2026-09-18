import QtQuick
import QtQuick.Window
import VintageAtlas 1.0

Window {
    id: root
    objectName: "meridianPreviewWindow"
    title: "Meridian — World Clock"
    visible: true
    width: 1600
    height: 700
    color: "#101714"
    property bool captureMode: appCaptureMode
    property bool saverMode: appSaverMode
    readonly property real rim: Math.max(7, Math.min(12, height * 0.006))
    readonly property real headingHeight: Math.max(24, Math.min(30, height * 0.023))
    readonly property real footerHeight: Math.max(22, Math.min(26, height * 0.021))
    readonly property real hourHeight: Math.max(18, Math.min(30, height * 0.023))
    readonly property real spaceWidth: width - rim * 2 - 8
    readonly property real spaceHeight: height - rim * 2 - headingHeight - footerHeight - hourHeight - 8
    readonly property bool wideMap: spaceWidth / Math.max(1, spaceHeight) > 2.6
    // Mild horizontal expansion makes better use of an ultrawide picture frame.
    readonly property real mapAspect: wideMap ? 3.4 : 2
    readonly property real mapWidth: Math.min(spaceWidth, spaceHeight * mapAspect)
    readonly property real mapHeight: mapWidth / mapAspect
    readonly property color brass: "#c0a06a"
    readonly property color ivory: "#e9dec1"

    Rectangle {
        id: frame
        objectName: "pictureFrame"
        anchors.centerIn: parent
        width: root.mapWidth + root.rim * 2
        height: root.mapHeight + root.hourHeight + root.headingHeight + root.footerHeight + root.rim * 2
        gradient: Gradient {
            GradientStop { position: 0; color: "#6c5034" }
            GradientStop { position: 0.08; color: "#38281c" }
            GradientStop { position: 0.92; color: "#302218" }
            GradientStop { position: 1; color: "#59412c" }
        }
        border.color: "#92774d"
        border.width: 1
        Rectangle {
            anchors.fill: parent
            anchors.margins: Math.max(3, root.rim * 0.3)
            color: "transparent"
            border.color: "#ad915d"
            border.width: 1
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: root.rim
            color: "#253630"
            Item {
                id: heading
                width: parent.width
                height: root.headingHeight
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "MERIDIAN"
                    color: root.ivory
                    font.family: "Noto Serif"
                    font.pixelSize: Math.max(13, root.headingHeight * 0.46)
                    font.letterSpacing: 3
                }
                Text {
                    anchors.centerIn: parent
                    visible: root.width >= 900
                    text: "A T L A S   O F   D A Y   &   N I G H T"
                    color: root.brass
                    font.family: "Noto Serif"
                    font.pixelSize: Math.max(10, root.headingHeight * 0.29)
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: clockModel.dateText
                    color: root.ivory
                    font.family: "Noto Sans"
                    font.pixelSize: Math.max(10, root.headingHeight * 0.29)
                }
            }
            AtlasMap {
                id: atlasMap
                objectName: "atlasMap"
                y: heading.height
                width: root.mapWidth
                height: root.mapHeight + root.hourHeight
                hourBandHeight: root.hourHeight
                equalArea: root.wideMap
                utc: clockModel.utc
            }
            Item {
                y: heading.height + atlasMap.height
                width: parent.width
                height: root.footerHeight
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "UTC  " + clockModel.utcText + "     /     LOCAL  " + clockModel.localText
                    color: root.ivory
                    font.family: "Noto Sans"
                    font.pixelSize: Math.max(10, root.footerHeight * 0.37)
                }
                Text {
                    anchors.centerIn: parent
                    visible: root.width >= 1100
                    text: "APPARENT SOLAR HOURS  /  " + (root.wideMap ? "EQUAL-AREA ATLAS" : "WORLD ATLAS")
                    color: root.brass
                    font.family: "Noto Serif"
                    font.italic: true
                    font.pixelSize: Math.max(10, root.footerHeight * 0.34)
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "ESC TO CLOSE"
                    color: root.brass
                    font.family: "Noto Sans"
                    font.pixelSize: Math.max(9, root.footerHeight * 0.3)
                }
            }
        }
    }
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Escape || root.saverMode) {
                root.close()
                event.accepted = true
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: root.saverMode
        onClicked: root.close()
    }
}

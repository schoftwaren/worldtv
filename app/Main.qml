import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Wtv

ApplicationWindow {
    id: root

    // ------------------------------------------------------------------
    // 10-foot couch UI for 1280x800 (Steam Deck). Driveable fully by
    // keyboard + gamepad (gamepad emits arrow/Return/Escape/F/... keys).
    // Views: Sidebar (countries/categories/favs) | channel grid, plus a
    // full-screen player overlay. Focus lives either on the lists (browse)
    // or on keyRoot (player / idle).
    // ------------------------------------------------------------------

    visible: false // show() from main after the size is applied
    // Explicit size, desktop only: without QML-declared width/height the
    // window gets auto-sized to content on desktop (came up 941x1030 under
    // xvfb; clobbered any resize() from main). WORLDTV_WINDOW_SIZE
    // overrides via QQmlProperty. Android keeps auto sizing (fullscreen
    // surface owns the geometry).
    width: Qt.platform.os === "android" ? implicitWidth : 1280
    height: Qt.platform.os === "android" ? implicitHeight : 800
    color: "#101418"
    font.pixelSize: 15

    // ---------------- design tokens ----------------
    QtObject {
        id: tk
        readonly property color bg: "#101418"
        readonly property color panel: "#181E24"
        readonly property color panel2: "#1F2730"
        readonly property color line: "#2A3540"
        readonly property color text: "#E8EDF2"
        readonly property color dim: "#8FA1B3"
        readonly property color accent: "#4FC3F7"
        readonly property color accentDim: "#265D75"
        readonly property color fav: "#FFC107"
        readonly property int radius: 10
        readonly property int cell: 150
        readonly property int cellH: 118
    }

    // ---------------- app state ----------------
    property string view: "browse"            // browse | player
    property bool searchOpen: false
    property var currentChannel: null
    property int streamIndex: 0
    property string playState: "idle"         // idle|connecting|playing|error
    property string playError: ""
    property bool overlayVisible: true
    property bool mpvReady: false     // mpvqt renderer-backed (GL) ready
    property bool mpvArmed: false     // core usable (renderer or not)
    property var pendingChannel: null
    property string debugState: ""

    function computeDebugState() {
        debugState = view + "/" + playState +
            (playError ? "/" + playError : "")
    }
    onPlayStateChanged: computeDebugState()
    onPlayErrorChanged: computeDebugState()
    Component.onCompleted:
        gridLoader.item.forceActiveFocus() // couch: grid highlight at launch
    onViewChanged: {
        computeDebugState()
        if (view === "player")
            keyRoot.forceActiveFocus()
        else if (gridLoader.item)
            gridLoader.item.forceActiveFocus()
    }

    // ---------------- playback ----------------
    function playChannel(row) {
        if (!row || !row.id)
            return
        Recent.add(row.id)
        if (!root.mpvArmed) {
            root.pendingChannel = row
            return
        }
        currentChannel = row
        streamIndex = 0
        startStream()
    }

    function startStream() {
        if (!currentChannel)
            return
        const urls = Store.streamUrls(currentChannel.id)
        if (streamIndex >= urls.length) {
            playState = "error"
            playError = "all " + urls.length + " streams failed"
            return
        }
        const s = urls[streamIndex]
        playState = "connecting"
        playError = ""
        overlayVisible = true
        view = "player"
        // per-stream HTTP options (some mirrors need UA/referrer)
        const headers = []
        if (s.ref)
            headers.push("Referer: " + s.ref)
        mpv.setProperty("http-header-fields", headers)
        mpv.setProperty("user-agent",
                        s.ua ? s.ua : "VLC/3.0.20 LibVLC/3.0.20")
        mpv.setProperty("network-timeout", 15)
        mpv.command(["loadfile", s.u])
        computeDebugState()
    }

    function nextMirror() {
        if (!currentChannel)
            return
        const urls = Store.streamUrls(currentChannel.id)
        if (streamIndex + 1 < urls.length) {
            streamIndex++
            startStream()
        } else {
            playState = "error"
            playError = "all " + urls.length + " streams failed"
        }
    }

    function stopPlayback() {
        mpv.command(["stop"])
        playState = "idle"
        playError = ""
        view = "browse"
        currentChannel = null
        overlayVisible = true
    }

    function demoPlay(channelId) {
        const row = Store.channelById(channelId)
        if (row && row.id)
            playChannel(row)
    }
    function demoBack() {
        stopPlayback()
    }

    // ---------------- global key surface ----------------
    // keyRoot holds focus in the player and whenever no list has it.
    Item {
        id: keyRoot
        focus: true
        anchors.fill: parent

        Keys.onPressed: (ev) => {
            if (root.view === "player") {
                if (ev.key === Qt.Key_Escape) {
                    root.stopPlayback()
                    ev.accepted = true
                } else if (ev.key === Qt.Key_Space) {
                    mpv.setProperty("pause", !mpv.getProperty("pause"))
                    ev.accepted = true
                } else if (ev.key === Qt.Key_M) {
                    mpv.setProperty("mute", !mpv.getProperty("mute"))
                    ev.accepted = true
                } else if (ev.key === Qt.Key_R) {
                    root.streamIndex = 0
                    root.startStream()
                    ev.accepted = true
                } else if (ev.key === Qt.Key_N) {
                    root.nextMirror()
                    ev.accepted = true
                } else if (ev.key === Qt.Key_PageUp ||
                           ev.key === Qt.Key_PageDown) {
                    const ids = Store.channels
                    if (ids.length === 0)
                        return
                    let cur = -1
                    for (let i = 0; i < ids.length; i++) {
                        if (root.currentChannel &&
                                ids[i].id === root.currentChannel.id) {
                            cur = i
                            break
                        }
                    }
                    const step = ev.key === Qt.Key_PageUp ? -1 : 1
                    const nx = ids[(cur + step + ids.length) % ids.length]
                    if (nx) {
                        root.currentChannel = nx
                        root.streamIndex = 0
                        root.startStream()
                    }
                    ev.accepted = true
                }
                if (ev.accepted)
                    root.overlayBump()
                return
            }
            // browse mode
            if (ev.key === Qt.Key_Slash && !root.searchOpen) {
                root.searchOpen = true
                searchField.forceActiveFocus()
                ev.accepted = true
            }
        }
    }

    function overlayBump() {
        overlayVisible = true
        overlayTimer.restart()
    }

    Timer {
        id: overlayTimer
        interval: 4000
        onTriggered:
            if (root.view === "player") root.overlayVisible = false
    }

    // ---------------- layout ----------------
    Row {
        anchors.fill: parent
        spacing: 0

        // ===== sidebar =====
        Rectangle {
            id: sidebar
            width: 320
            height: parent.height
            color: tk.panel

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 72
                    Label {
                        anchors.centerIn: parent
                        text: "🌍 WORLD TV"
                        color: tk.accent
                        font.pixelSize: 26
                        font.bold: true
                    }
                }

                Rectangle {
                    Layout.margins: 12
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    radius: tk.radius
                    color: tk.panel2
                    border.color: searchField.activeFocus ? tk.accent : tk.line
                    border.width: searchField.activeFocus ? 2 : 1
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        Label { text: "🔍"; font.pixelSize: 18 }
                        TextInput {
                            id: searchField
                            Layout.fillWidth: true
                            color: tk.text
                            font.pixelSize: 16
                            clip: true
                            verticalAlignment: TextInput.AlignVCenter
                            onTextEdited: Store.search = text
                            Keys.onEscapePressed: (ev) => {
                                if (text.length > 0) {
                                    text = ""
                                    Store.search = ""
                                } else {
                                    root.searchOpen = false
                                    keyRoot.forceActiveFocus()
                                }
                                ev.accepted = true
                            }
                            Keys.onReturnPressed: (ev) => {
                                gridLoader.item.forceActiveFocus()
                                ev.accepted = true
                            }
                        }
                        Label {
                            visible: searchField.text.length > 0
                            text: "✕"
                            color: tk.dim
                            TapHandler {
                                onTapped: {
                                    searchField.text = ""
                                    Store.search = ""
                                }
                            }
                        }
                    }
                }

                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    keyNavigationEnabled: true
                    highlightFollowsCurrentItem: true
                    model: navModel
                    currentIndex: 0

                    delegate: ItemDelegate {
                        id: navDel
                        width: navList.width
                        height: 46
                        focus: false
                        highlighted: ListView.isCurrentItem
                        background: Rectangle {
                            radius: 8
                            color: navDel.ListView.isCurrentItem
                                       ? (navList.activeFocus
                                          ? tk.accentDim : tk.panel2)
                                       : "transparent"
                            border.width: navDel.ListView.isCurrentItem &&
                                          navList.activeFocus ? 2 : 0
                            border.color: tk.accent
                        }
                        contentItem: RowLayout {
                            spacing: 10
                            Label {
                                text: model.flag
                                font.pixelSize: 22
                                Layout.preferredWidth: 34
                            }
                            Label {
                                text: model.label
                                color: tk.text
                                font.pixelSize: 16
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: model.count > 0 ? model.count : ""
                                color: tk.dim
                                font.pixelSize: 13
                            }
                        }
                        onClicked: navList.currentIndex = index
                    }

                    function applyNav() {
                        const it = navModel.get(navList.currentIndex)
                        if (!it)
                            return
                        Store.filterFav = false
                        Store.filterCountry = ""
                        Store.filterCategory = ""
                        if (it.kind === "country")
                            Store.filterCountry = it.cc
                        else if (it.kind === "fav")
                            Store.filterFav = true
                        else if (it.kind === "cat")
                            Store.filterCategory = it.cc
                        gridLoader.item.forceActiveFocus()
                        gridLoader.item.positionViewAtBeginning()
                    }
                    Keys.onReturnPressed: (ev) => {
                        applyNav(); ev.accepted = true
                    }
                    Keys.onSpacePressed: (ev) => {
                        applyNav(); ev.accepted = true
                    }
                    Keys.onEscapePressed: (ev) => {
                        if (root.searchOpen) {
                            root.searchOpen = false
                            searchField.text = ""
                            Store.search = ""
                            keyRoot.forceActiveFocus()
                            ev.accepted = true
                        }
                    }
                }
            }
        }

        // ===== content =====
        Rectangle {
            id: content
            width: parent.width - sidebar.width
            height: parent.height
            color: tk.bg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 56
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        Label {
                            text: {
                                if (Store.filterFav)
                                    return "★ Favorites"
                                if (Store.filterCountry !== "") {
                                    for (let i = 0; i < navModel.count; i++) {
                                        const it = navModel.get(i)
                                        if (it.kind === "country" &&
                                                it.cc === Store.filterCountry)
                                            return it.flag + " " + it.label
                                    }
                                }
                                if (Store.filterCategory !== "")
                                    return "◼ " + Store.filterCategory
                                if (Store.search !== "")
                                    return "🔍 " + Store.search
                                return "All channels"
                            }
                            color: tk.text
                            font.pixelSize: 20
                            font.bold: true
                        }
                        Label {
                            text: Store.channels.length + " channels"
                            color: tk.dim
                            font.pixelSize: 14
                        }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: "↵ play   F fav   / search   ESC back"
                            color: tk.dim
                            font.pixelSize: 13
                        }
                    }
                    Rectangle {
                        width: parent.width
                        height: 1
                        anchors.bottom: parent.bottom
                        color: tk.line
                    }
                }

                Loader {
                    id: gridLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: gridComp
                }

                Component {
                    id: gridComp
                    GridView {
                        id: grid
                        clip: true
                        cellWidth: tk.cell + 14
                        cellHeight: tk.cellH + 18
                        model: gridModel
                        keyNavigationEnabled: true
                        highlightFollowsCurrentItem: true
                        currentIndex: 0
                        ScrollBar.vertical: ScrollBar {}

                        function rowsVisible() {
                            return Math.max(1, Math.floor(
                                grid.width / grid.cellWidth))
                        }
                        Keys.onReturnPressed: (ev) => {
                            const row = gridModel.get(currentIndex)
                            if (row)
                                root.playChannel(row)
                            ev.accepted = true
                        }
                        Keys.onPressed: (ev) => {
                            if (ev.key === Qt.Key_F) {
                                const row = gridModel.get(currentIndex)
                                if (row)
                                    Favs.toggle(row.id)
                                ev.accepted = true
                            } else if (ev.key === Qt.Key_Slash) {
                                root.searchOpen = true
                                searchField.forceActiveFocus()
                                ev.accepted = true
                            }
                        }
                        Keys.onLeftPressed: (ev) => {
                            if (currentIndex % grid.rowsVisible() === 0 &&
                                    Store.search === "") {
                                navList.forceActiveFocus()
                                ev.accepted = true
                            }
                        }
                        Keys.onEscapePressed: (ev) => {
                            if (Store.search !== "" ||
                                    Store.filterCountry !== "" ||
                                    Store.filterCategory !== "" ||
                                    Store.filterFav) {
                                Store.search = ""
                                Store.filterCountry = ""
                                Store.filterCategory = ""
                                Store.filterFav = false
                                searchField.text = ""
                                ev.accepted = true
                            }
                        }

                        delegate: ItemDelegate {
                            width: grid.cellWidth - 14
                            height: grid.cellHeight - 18
                            focus: false
                            highlighted: GridView.isCurrentItem

                            background: Rectangle {
                                radius: tk.radius
                                color: tk.panel
                                // couch affordance: current tile always
                                // marked, brighter when the grid owns focus
                                border.color: GridView.isCurrentItem
                                                  ? (grid.activeFocus
                                                         ? tk.accent
                                                         : tk.accentDim)
                                                  : tk.line
                                border.width: GridView.isCurrentItem ? 2 : 1
                            }

                            contentItem: ColumnLayout {
                                spacing: 4
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: tk.cellH - 34
                                    radius: 6
                                    color: tk.panel2
                                    clip: true

                                    Image {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        source: model.logo
                                        fillMode: Image.PreserveAspectFit
                                        asynchronous: true
                                        visible: model.logo.length > 0 &&
                                                 status !== Image.Error
                                    }
                                    Label {
                                        anchors.centerIn: parent
                                        visible: model.logo.length === 0
                                        text: model.flag
                                        font.pixelSize: 34
                                    }
                                    Rectangle {
                                        visible: model.quality.length > 0
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 5
                                        radius: 4
                                        color: "#CC000000"
                                        width: qLabel.implicitWidth + 10
                                        height: qLabel.implicitHeight + 4
                                        Label {
                                            id: qLabel
                                            anchors.centerIn: parent
                                            text: model.quality
                                            color: "#9FE8FF"
                                            font.pixelSize: 10
                                        }
                                    }
                                    Rectangle {
                                        visible: model.nUrls > 1
                                        anchors.left: parent.left
                                        anchors.bottom: parent.bottom
                                        anchors.margins: 5
                                        radius: 4
                                        color: "#CC000000"
                                        width: mLabel.implicitWidth + 10
                                        height: mLabel.implicitHeight + 4
                                        Label {
                                            id: mLabel
                                            anchors.centerIn: parent
                                            text: model.nUrls + " feeds"
                                            color: tk.dim
                                            font.pixelSize: 10
                                        }
                                    }
                                    Label {
                                        visible: Favs.isFav(model.id)
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.margins: 5
                                        text: "★"
                                        color: tk.fav
                                        font.pixelSize: 16
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: tk.text
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                            onClicked: {
                                grid.currentIndex = index
                                root.playChannel(gridModel.get(index))
                            }
                        }
                        // mpvqt emits ready() only from its GL renderer; in
                        // offscreen/CI runs there is none, so arm the core
                        // and resume pending autoplay from here.
                        Component.onCompleted: {
                            root.mpvArmed = true
                            if (root.pendingChannel) {
                                const row = root.pendingChannel
                                root.pendingChannel = null
                                root.playChannel(row)
                            }
                        }
                    }
                }

                // reactive filtered slice -> grid model
                ListModel {
                    id: gridModel
                    function sync() {
                        const rows = Store.channels
                        clear()
                        for (let i = 0; i < rows.length; i++)
                            append(rows[i])
                    }
                    Component.onCompleted: sync()
                }
                Connections {
                    target: Store
                    function onChannelsChanged() { gridModel.sync() }
                }
                Connections {
                    target: Favs
                    function onChanged() { gridModel.sync() }
                }
            }
        }
    }

    // ================= PLAYER OVERLAY =================
    Rectangle {
        id: playerLayer
        anchors.fill: parent
        visible: root.view === "player"
        color: "#000000"

        MpvItem {
            id: mpv
            anchors.fill: parent
            visible: root.view === "player"
            onReady: {
                root.mpvReady = true
                setProperty("keep-open", "no")
                setProperty("osc", "no")
                setProperty("fullscreen", "no")
                setProperty("hwdec", "auto-safe")
                setProperty("cache", "yes")
                setProperty("cache-secs", 30)
                setProperty("volume", 100)
                if (root.pendingChannel) {
                    const row = root.pendingChannel
                    root.pendingChannel = null
                    root.playChannel(row)
                }
            }
            onPropChanged: (prop, value) => {
                if (prop === "pause")
                    root.paused = value
            }
            onPlaybackLive: root.playState = "playing"
            onFileEnded: (reason) => {
                if (reason === "error")
                    root.nextMirror()
                else if (reason === "eof" && root.currentChannel)
                    root.startStream() // live stream ended: reconnect
            }
        }

        Rectangle {
            id: statusBanner
            visible: root.playState !== "playing" &&
                     root.view === "player"
            anchors.centerIn: parent
            width: Math.min(560, parent.width - 80)
            height: statusCol.implicitHeight + 48
            radius: tk.radius
            color: "#E612181E"
            border.color: root.playState === "error" ? "#E05555" : tk.accent
            border.width: 1

            ColumnLayout {
                id: statusCol
                anchors.centerIn: parent
                spacing: 10
                width: parent.width - 48

                Label {
                    property string chLine:
                        root.currentChannel
                            ? (root.currentChannel.flag || "") + "  " +
                              root.currentChannel.name
                            : ""
                    text: chLine
                    color: tk.text
                    font.pixelSize: 22
                    font.bold: true
                    Layout.fillWidth: true
                }
                Label {
                    text: root.playState === "error"
                              ? "✕ " + root.playError
                              : "◌ connecting…  (feed " +
                                (root.streamIndex + 1) + ")"
                    color: root.playState === "error" ? "#FF8888" : tk.dim
                    font.pixelSize: 15
                    Layout.fillWidth: true
                }
                Label {
                    visible: root.playState === "error"
                    text: "N next feed   ·   R retry   ·   ESC back"
                    color: tk.dim
                    font.pixelSize: 13
                }
                BusyIndicator {
                    visible: root.playState === "connecting"
                    running: visible
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        Rectangle {
            id: infoBar
            visible: root.overlayVisible && root.view === "player"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 64
            gradient: Gradient {
                GradientStop { position: 0; color: "#CC000000" }
                GradientStop { position: 1; color: "#00000000" }
            }
            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                Label {
                    text: root.currentChannel
                              ? (root.currentChannel.flag || "") + "  " +
                                root.currentChannel.name
                              : ""
                    color: "#FFFFFF"
                    font.pixelSize: 20
                    font.bold: true
                }
                Label {
                    text: root.currentChannel &&
                          root.currentChannel.countryName
                              ? "· " + root.currentChannel.countryName
                              : ""
                    color: "#B0C4D4"
                    font.pixelSize: 15
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: root.currentChannel && root.currentChannel.nUrls > 1
                              ? "N next feed (" + (root.streamIndex + 1) + "/" +
                                root.currentChannel.nUrls + ")   ESC back"
                              : "ESC back"
                    color: "#B0C4D4"
                    font.pixelSize: 13
                }
            }
        }
    }

    // ================= NAV MODEL =================
    ListModel {
        id: navModel
        Component.onCompleted: build()
        function build() {
            if (Store.channelCount === 0) {
                Qt.callLater(build) // catalog not parsed yet
                return
            }
            clear()
            append({ kind: "all", flag: "🌍", label: "All channels",
                     count: Store.channelCount, cc: "" })
            append({ kind: "fav", flag: "★", label: "Favorites",
                     count: Favs.count, cc: "" })
            const cs = Store.countries
            for (let i = 0; i < cs.length; i++)
                append({ kind: "country", flag: cs[i].flag,
                         label: cs[i].name, count: cs[i].n, cc: cs[i].cc })
            const cats = Store.categories
            for (let j = 0; j < cats.length; j++)
                append({ kind: "cat", flag: "◼", label: cats[j].id,
                         count: cats[j].n, cc: cats[j].id })
        }
    }
    Connections {
        target: Store
        function onLoadedChanged() { navModel.build() }
    }
    Connections {
        target: Favs
        function onChanged() {
            if (navModel.count > 1)
                navModel.setProperty(1, "count", Favs.count)
        }
    }

    property bool paused: false
}

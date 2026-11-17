import QtQuick

// The table: the piles of the game in play and the cards that move between
// them. The cards are the same items for the whole game, so a move is a card
// gliding from one place to the next.
Item {
  id: table
  property var look
  // The table reads left to right in every language, like the cards themselves:
  // the stock on the left, each card's rank before its suit.
  LayoutMirroring.enabled: false
  LayoutMirroring.childrenInherit: true
  // The keyboard's place on the table, shown only once the keyboard is used.
  // It starts on the first column, which the game in play decides.
  property bool keyboard: false
  property int cursorPile: backend.tableauStart
  property int cursorIndex: 0
  // The run of cards being dragged, and how far from where it lay.
  property bool dragging: false
  property int dragPile: -1
  property int dragIndex: -1
  property real dragDx: 0
  property real dragDy: 0
  // Cards glide when the game moves them, not when the window is resized.
  property bool resizing: false
  // The tests turn this off, to find every card where it belongs at once.
  property bool motion: true
  readonly property bool animate: motion && !dragging && !resizing

  readonly property real sideMargin: Math.max(18, width * .04)
  readonly property real topMargin: Math.max(18, Math.min(32, height * .04))
  // The number of columns the bottom row holds, which the two games differ on.
  readonly property int columns: backend.pileCount - backend.tableauStart
  // As wide as the bottom row's columns allow, and no taller than leaves room
  // for the longest column there can be with every rank still readable. The
  // first divisor is the columns plus the gaps between them; the second is the
  // rows of cards the height must hold.
  readonly property real cardWidth: Math.max(36, Math.min((width - 2 * sideMargin) / (columns + (columns - 1) * .18), (height - 2 * topMargin) / 6.2))
  readonly property real cardHeight: cardWidth * 1.39
  readonly property real unit: cardWidth / 112
  readonly property real gap: cardWidth * .18
  // The columns sit centred, however many of them the game in play lays out.
  readonly property real originX: (width - columns * cardWidth - (columns - 1) * gap) / 2
  readonly property real tableauTop: topMargin + cardHeight + cardHeight * .23
  readonly property real downStep: cardHeight * .1
  readonly property real upStep: cardHeight * .23
  // How far each of the waste's last three cards lies from the one before.
  readonly property real fanStep: .36

  onWidthChanged: { resizing = true; settle.restart() }
  onHeightChanged: { resizing = true; settle.restart() }
  Timer { id: settle; interval: 120; onTriggered: table.resizing = false }

  function pileX(pile) {
    // Klondike: the stock takes column 0, the waste column 1, the foundations
    // the next four, and the seven columns the bottom row.
    if (backend.mode === 0) {
      const column = pile === 0 ? 0 : pile === 1 ? 1 : pile < 6 ? pile + 1 : pile - 6
      return originX + column * (cardWidth + gap)
    }
    // Spider: the stock takes column 0, the eight homes sit on columns 1 to 8,
    // and the ten columns take the bottom row under them.
    const column = pile === 0 ? 0 : pile < backend.tableauStart ? pile : pile - backend.tableauStart
    return originX + column * (cardWidth + gap)
  }
  function pileY(pile) { return pile < backend.tableauStart ? topMargin : tableauTop }
  // A long column closes up so its last card stays on the table. Worked out
  // once per pile, not once per card.
  readonly property var squeezes: backend.piles.map((info) => {
    const wanted = info.down * downStep + Math.max(0, info.up - 1) * upStep
    const room = height - topMargin - tableauTop - cardHeight
    return wanted > room && wanted > 0 ? Math.max(.2, room / wanted) : 1
  })
  function squeeze(pile) { return squeezes[pile] }
  function cardX(info) { return pileX(info.pile) + (info.pile === backend.waste ? info.fan * cardWidth * fanStep : 0) }
  function cardY(info) {
    if (info.pile < backend.tableauStart) return topMargin
    return tableauTop + (info.down * downStep + info.upBelow * upStep) * squeeze(info.pile)
  }
  // Where the cards from `index` to the top of `pile` lie, as one outline.
  function runRect(pile, index) {
    if (pile < 0) return Qt.rect(0, 0, 0, 0)
    const info = backend.piles[pile]
    if (pile < backend.tableauStart) {
      const fanned = pile === backend.waste ? Math.max(0, Math.min(info.count, 3) - 1) : 0
      return Qt.rect(pileX(pile) + fanned * cardWidth * fanStep, topMargin, cardWidth, cardHeight)
    }
    const place = (at) => (at < info.down ? at * downStep : info.down * downStep + (at - info.down) * upStep) * squeeze(pile)
    const first = Math.max(0, index)
    return Qt.rect(pileX(pile), tableauTop + place(first), cardWidth, place(Math.max(first, info.count - 1)) - place(first) + cardHeight)
  }
  function draggable(pile, index, faceUp) {
    return faceUp && pile > 0 && !backend.won && (pile >= backend.tableauStart || index === backend.piles[pile].count - 1)
  }
  function endDrag(item) {
    const centreX = item.x + cardWidth / 2
    const centreY = item.y + cardHeight / 2
    // Above the tableau a run is offered to the home piles, which start after
    // the stock: the foundations for Klondike, the finished-run piles for
    // Spider. Below it is offered to the columns.
    const home = centreY < tableauTop - cardHeight * .12
    const first = home ? (backend.mode === 0 ? 2 : 1) : backend.tableauStart
    const last = home ? backend.tableauStart - 1 : backend.pileCount - 1
    let nearest = -1
    for (let pile = first; pile <= last; ++pile)
      if (nearest < 0 || Math.abs(pileX(pile) + cardWidth / 2 - centreX) < Math.abs(pileX(nearest) + cardWidth / 2 - centreX)) nearest = pile
    const fromPile = dragPile, fromIndex = dragIndex
    // Animations come back on first, so the cards glide from where they were let go.
    dragging = false
    if (Math.abs(pileX(nearest) + cardWidth / 2 - centreX) < cardWidth * .8) backend.drop(fromPile, fromIndex, nearest)
    dragPile = -1
    dragIndex = -1
    dragDx = 0
    dragDy = 0
  }

  // A drag taken away rather than let go puts the cards back where they were.
  function cancelDrag() {
    dragging = false
    dragPile = -1
    dragIndex = -1
    dragDx = 0
    dragDy = 0
  }
  // When a click last put cards down, so the second click of a double click
  // on the same spot doesn't send them on somewhere else.
  property double placedAt: 0
  property int placedOn: -1
  function click(pile, index) {
    const before = backend.moves
    backend.activate(pile, index)
    if (pile !== 0 && backend.moves !== before) {
      placedAt = Date.now()
      placedOn = pile
    }
  }

  function topIndex(pile) { return backend.piles[pile].count - 1 }
  function clampCursor() {
    // The cursor outlives a game switch, so it is brought back onto the
    // table before anything reads where it points.
    cursorPile = Math.max(0, Math.min(backend.pileCount - 1, cursorPile))
    const info = backend.piles[cursorPile]
    if (info.count === 0) cursorIndex = -1
    // On a top-row pile only the last card can be marked; in a column, the
    // first face-up one or later.
    else if (cursorPile < backend.tableauStart) cursorIndex = info.count - 1
    else cursorIndex = Math.max(info.down, Math.min(info.count - 1, cursorIndex < 0 ? info.count - 1 : cursorIndex))
  }
  function navigate(dx, dy) {
    keyboard = true
    clampCursor()
    if (backend.mode === 0) {
      // Klondike's two rows: the stock, waste and foundations on top, the
      // seven columns below. Walking off a row stops at its end.
      if (dx !== 0) {
        const first = cursorPile < 6 ? 0 : 6, last = cursorPile < 6 ? 5 : 12
        cursorPile = Math.max(first, Math.min(last, cursorPile + dx))
        cursorIndex = topIndex(cursorPile)
      } else if (dy < 0) {
        if (cursorPile >= 6 && cursorIndex > backend.piles[cursorPile].down) {
          --cursorIndex
        } else if (cursorPile >= 6) {
          const column = cursorPile - 6
          cursorPile = column === 0 ? 0 : column < 3 ? 1 : column - 1
          cursorIndex = topIndex(cursorPile)
        }
      } else if (cursorPile < 6) {
        cursorPile = cursorPile === 0 ? 6 : cursorPile === 1 ? 7 : cursorPile + 7
        cursorIndex = topIndex(cursorPile)
      } else if (cursorIndex < topIndex(cursorPile)) {
        ++cursorIndex
      }
    } else {
      // Spider's two rows: the stock and the eight homes on top, the ten
      // columns below. Walking off a row stops at its end.
      if (dx !== 0) {
        const first = cursorPile < backend.tableauStart ? 0 : backend.tableauStart
        const last = cursorPile < backend.tableauStart ? backend.tableauStart - 1 : backend.pileCount - 1
        cursorPile = Math.max(first, Math.min(last, cursorPile + dx))
        cursorIndex = topIndex(cursorPile)
      } else if (dy < 0) {
        if (cursorPile >= backend.tableauStart && cursorIndex > backend.piles[cursorPile].down) {
          --cursorIndex
        } else if (cursorPile >= backend.tableauStart) {
          // Up from a column goes to the top-row pile over it: the stock for
          // column 0, the home sitting on that column for the rest, and for
          // the tenth column, which has no home of its own, the home beside it.
          const column = cursorPile - backend.tableauStart
          cursorPile = column === 0 ? 0 : Math.min(column, backend.tableauStart - 1)
          cursorIndex = topIndex(cursorPile)
        }
      } else if (cursorPile < backend.tableauStart) {
        // Down from the stock goes to the first column; from a home, to the
        // column under it.
        cursorPile = cursorPile === 0 ? backend.tableauStart : backend.tableauStart + cursorPile
        cursorIndex = topIndex(cursorPile)
      } else if (cursorIndex < topIndex(cursorPile)) {
        ++cursorIndex
      }
    }
    clampCursor()
  }
  function keyboardSelect() {
    keyboard = true
    clampCursor()
    backend.activate(cursorPile, cursorIndex)
  }
  function keyboardSend() {
    keyboard = true
    clampCursor()
    backend.autoPlay(cursorPile, cursorIndex)
  }
  Connections {
    target: backend
    function onChanged() { table.clampCursor() }
  }

  Rectangle {
    anchors.fill: parent
    color: table.look.table
    Canvas {
      id: ground
      anchors.fill: parent
      visible: table.look.pattern !== ""
      property string pattern: table.look.pattern
      property color ink: table.look.patternColor
      // A canvas redraws itself when it changes size, and only while it shows.
      onPatternChanged: if (visible) requestPaint()
      onInkChanged: if (visible) requestPaint()
      onVisibleChanged: if (visible) requestPaint()
      onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.fillStyle = ink
        if (pattern === "dots") {
          for (let y = 10; y < height; y += 20)
            for (let x = 10; x < width; x += 20) ctx.fillRect(x, y, 1.5, 1.5)
        } else if (pattern === "stars") {
          // The same sky every time: a fixed sequence stands in for chance.
          let state = 7
          const next = () => (state = (state * 1103515245 + 12345) % 2147483648) / 2147483648
          for (let star = 0; star < width * height / 5200; ++star) {
            const x = next() * width, y = next() * height, size = next()
            ctx.globalAlpha = .25 + size * .6
            ctx.beginPath()
            ctx.arc(x, y, size < .9 ? .9 : 1.6, 0, Math.PI * 2)
            ctx.fill()
          }
        }
      }
    }
    MouseArea { anchors.fill: parent; onClicked: { table.keyboard = false; backend.cancelSelection() } }
  }

  Repeater {
    model: backend.pileCount
    Rectangle {
      id: slot
      required property int index
      readonly property bool empty: backend.piles[index].count === 0
      objectName: "slot-" + index
      x: table.pileX(index)
      y: table.pileY(index)
      width: table.cardWidth
      height: table.cardHeight
      radius: table.look.radius * table.unit
      color: table.look.slotFill
      border.color: table.look.slot
      border.width: table.look.slotWidth
      // Each Klondike foundation shows the suit it collects; a Spider home
      // may end in any suit, so it shows nothing.
      Suit {
        visible: backend.mode === 0 && slot.index >= 2 && slot.index < 6
        anchors.centerIn: parent
        suit: slot.index >= 2 && slot.index < 6 ? slot.index - 2 : 0
        color: table.look.mark
        outline: table.look.centreOutline
        size: 40 * table.unit
        stroke: 1.2
      }
      // An empty stock shows a ring: a click brings the waste back round.
      Rectangle {
        visible: slot.index === 0
        anchors.centerIn: parent
        width: 34 * table.unit
        height: width
        radius: width / 2
        color: "transparent"
        border.color: table.look.mark
        border.width: Math.max(1, 2 * table.unit)
      }
      Rectangle {
        visible: slot.empty && backend.targets.indexOf(slot.index) >= 0
        anchors.fill: parent
        anchors.margins: -5 * table.unit
        radius: (table.look.radius + 5) * table.unit
        color: "transparent"
        border.color: table.look.ring
        border.width: Math.max(1, 1.5 * table.unit)
      }
      Rectangle {
        visible: slot.empty && table.keyboard && table.cursorPile === slot.index
        anchors.fill: parent
        anchors.margins: -8 * table.unit
        radius: (table.look.radius + 8) * table.unit
        color: "transparent"
        border.color: table.look.cursor
        border.width: Math.max(1, 1.5 * table.unit)
      }
      MouseArea {
        anchors.fill: parent
        onClicked: { table.keyboard = false; table.click(slot.index, -1) }
      }
    }
  }

  Repeater {
    model: backend.cardsTotal
    PlayingCard {
      id: item
      required property int index
      readonly property var info: backend.cards[index]
      readonly property int pile: info.pile
      readonly property int place: info.index
      readonly property bool onTop: place === backend.piles[pile].count - 1
      readonly property bool dragged: table.dragPile === pile && place >= table.dragIndex
      objectName: "card-" + index
      cardId: index
      faceUp: info.up
      // The table passes the suit along, since in a one- or two-suit deal the
      // id alone names the wrong one.
      suit: info.suit
      look: table.look
      unit: table.unit
      motion: table.animate
      width: table.cardWidth
      height: table.cardHeight
      x: table.cardX(info) + (dragged ? table.dragDx : 0)
      y: table.cardY(info) + (dragged ? table.dragDy : 0)
      z: place + (dragged ? 200 : glideX.running || glideY.running ? 100 : 0)
      lifted: dragged
      target: onTop && backend.targets.indexOf(pile) >= 0
      cursor: table.keyboard && table.cursorPile === pile && table.cursorIndex === place
      Behavior on x { enabled: table.animate; NumberAnimation { id: glideX; duration: 190; easing.type: Easing.OutCubic } }
      Behavior on y { enabled: table.animate; NumberAnimation { id: glideY; duration: 190; easing.type: Easing.OutCubic } }
      MouseArea {
        objectName: "grip"
        anchors.fill: parent
        // A drag keeps hold of the card even as it leaves where it was pressed.
        preventStealing: true
        property real startX: 0
        property real startY: 0
        property bool moved: false
        function finish() {
          if (moved && table.dragPile >= 0) table.endDrag(item)
        }
        onPressed: (mouse) => {
          table.keyboard = false
          const point = mapToItem(table, mouse.x, mouse.y)
          startX = point.x
          startY = point.y
          moved = false
        }
        onPositionChanged: (mouse) => {
          if (!pressed) return
          const point = mapToItem(table, mouse.x, mouse.y)
          if (!moved && Math.hypot(point.x - startX, point.y - startY) > 6 && table.draggable(item.pile, item.place, item.faceUp)) {
            moved = true
            backend.cancelSelection()
            table.dragging = true
            table.dragPile = item.pile
            table.dragIndex = item.place
          }
          if (moved) {
            table.dragDx = point.x - startX
            table.dragDy = point.y - startY
          }
        }
        onReleased: finish()
        onCanceled: if (moved) table.cancelDrag()
        onClicked: if (!moved) table.click(item.pile, item.place)
        onDoubleClicked: if (!moved && !(item.pile === table.placedOn && Date.now() - table.placedAt < 600)) backend.autoPlay(item.pile, item.place)
      }
    }
  }

  // One outline around everything picked up, and around each end of a hint,
  // which breathes until a move is made.
  component Outline: Rectangle {
    property rect around
    property bool breathing: false
    x: around.x - 3 * table.unit
    y: around.y - 3 * table.unit
    width: around.width + 6 * table.unit
    height: around.height + 6 * table.unit
    z: 1000
    radius: (table.look.radius + 3) * table.unit
    color: "transparent"
    border.color: table.look.ring
    border.width: Math.max(2, 2.5 * table.unit)
    SequentialAnimation on opacity {
      // Only while the window is in front, so a hint left up doesn't keep it drawing.
      running: breathing && visible && table.motion && table.Window.active
      loops: Animation.Infinite
      NumberAnimation { to: .25; duration: 520; easing.type: Easing.InOutSine }
      NumberAnimation { to: 1; duration: 520; easing.type: Easing.InOutSine }
    }
  }
  Outline { objectName: "selectionRing"; visible: backend.selectedPile >= 0; around: table.runRect(backend.selectedPile, backend.selectedIndex) }
  Outline { objectName: "hintRing"; visible: backend.hintFrom >= 0; breathing: true; around: table.runRect(backend.hintFrom, backend.hintIndex) }
  Outline { objectName: "hintTargetRing"; visible: backend.hintTo >= 0; breathing: true; around: backend.hintTo >= 0 ? table.runRect(backend.hintTo, backend.piles[backend.hintTo].count - 1) : Qt.rect(0, 0, 0, 0) }
}

import AppKit
import QuartzCore

final class PetPanel: NSPanel {
    override var canBecomeKey: Bool { false }
    override var canBecomeMain: Bool { false }
}

final class PetView: NSView {
    weak var controller: AppController?
    var character: Character?
    var sprite: Sprite?
    var previous: Sprite?
    var changedAt: Double = 0
    var now: Double = 0
    var motionTime: Double = 0
    var pose: Pose = .idle
    var scale: CGFloat = 1
    var quiet = false
    var frozen = false
    var reduced = false
    var bubble = ""
    var landingAt: Double = -100
    var dragging = false
    var dragStart = NSPoint.zero
    var originalOrigin = NSPoint.zero
    var lean: CGFloat = 0
    var spriteRect = NSRect.zero
    private var pressed = false
    private let imageLayer = CALayer()
    private let oldImageLayer = CALayer()
    private let shadowLayer = CAShapeLayer()
    private let bubbleLayer = CALayer()
    private let textLayer = CATextLayer()
    private let tailLayer = CAShapeLayer()

    init(character: Character?, controller: AppController) {
        self.character = character; self.controller = controller
        super.init(frame: .zero)
        wantsLayer = true
        layer?.backgroundColor = NSColor.clear.cgColor
        for child in [shadowLayer, oldImageLayer, imageLayer, tailLayer, bubbleLayer] { layer?.addSublayer(child) }
        for child in [imageLayer, oldImageLayer] { child.contentsGravity = .resizeAspect; child.magnificationFilter = .linear; child.minificationFilter = .trilinear }
        shadowLayer.fillColor = NSColor(calibratedWhite: 0.15, alpha: 0.065).cgColor
        bubbleLayer.backgroundColor = NSColor(calibratedRed: 1, green: 0.975, blue: 0.91, alpha: 0.98).cgColor
        bubbleLayer.cornerRadius = 12
        tailLayer.fillColor = bubbleLayer.backgroundColor
        textLayer.font = NSFont.systemFont(ofSize: 12.5, weight: .medium)
        textLayer.fontSize = 12.5
        textLayer.alignmentMode = .center
        textLayer.foregroundColor = NSColor(calibratedRed: 0.29, green: 0.22, blue: 0.18, alpha: 1).cgColor
        bubbleLayer.addSublayer(textLayer)
        setAccessibilityElement(true)
        setAccessibilityRole(.button)
        setAccessibilityLabel(character?.name ?? "两个小伙伴")
        setAccessibilityHelp("点击打招呼，拖动移动，右键打开动作菜单。")
    }
    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }
    override var isOpaque: Bool { false }
    override func accessibilityPerformPress() -> Bool {
        controller?.touched(character); return true
    }
    override func accessibilityPerformShowMenu() -> Bool {
        DispatchQueue.main.async { [weak self] in
            guard let self, let menu = self.controller?.contextMenu(self.character) else { return }
            menu.popUp(positioning: nil, at: NSPoint(x: self.bounds.midX, y: self.spriteRect.midY), in: self)
        }
        return true
    }

    func show(_ next: Sprite, key: Int) {
        if currentKey != key {
            previous = sprite; sprite = next; changedAt = now; currentKey = key
            CATransaction.begin(); CATransaction.setDisableActions(true)
            oldImageLayer.contents = previous?.cgImage; imageLayer.contents = next.cgImage
            CATransaction.commit()
        }
    }
    private var currentKey = -1

    var breath: CGFloat {
        frozen || reduced ? 0 : CGFloat(sin(motionTime * (pose == .sleep ? 1.3 : 2.1))) * (quiet ? 0.6 : 1.1)
    }

    func rect(for value: Sprite) -> NSRect {
        let factor = 208 * scale / value.referenceWidth
        let w = CGFloat(value.width) * factor, h = CGFloat(value.height) * factor
        var bounce: CGFloat = 0
        if !reduced && !frozen {
            if pose == .wave || pose == .love { bounce = abs(CGFloat(sin(motionTime * 4))) * 3 * scale }
            if dragging { bounce = CGFloat(sin(motionTime * 5)) * 3 * scale }
        }
        let elapsed = now - landingAt
        let squash: CGFloat = elapsed >= 0 && elapsed < 0.45 && !reduced ? CGFloat(sin(elapsed / 0.45 * .pi)) * 0.075 : 0
        return NSRect(x: (bounds.width - w * (1 + squash))/2 + lean, y: 16*scale + bounce,
                      width: w * (1 + squash), height: h * (1 - squash) + breath)
    }

    func opaqueAt(_ point: NSPoint) -> Bool {
        guard let sprite else { return false }
        let r = rect(for: sprite)
        return sprite.contains(u: (point.x-r.minX)/r.width, v: (point.y-r.minY)/r.height)
    }

    /// Cached CGImages stay in Core Animation; per-frame work only changes layer geometry.
    func render() {
        guard let sprite else { return }
        let r = rect(for: sprite); spriteRect = r
        CATransaction.begin(); CATransaction.setDisableActions(true)
        shadowLayer.isHidden = dragging
        shadowLayer.path = CGPath(ellipseIn: NSRect(x: bounds.midX-r.width*0.26, y: 11*scale, width: r.width*0.52, height: 7*scale), transform: nil)
        let mix = reduced ? 1 : min(1, max(0, (now-changedAt) / 0.14))
        let angle: CGFloat = reduced || frozen ? 0 : (dragging ? CGFloat(sin(motionTime*4))*4 : (pose == .grumpy ? CGFloat(sin(motionTime*7))*1.2 : lean*0.25))
        imageLayer.setAffineTransform(.identity)
        imageLayer.frame = r
        imageLayer.setAffineTransform(CGAffineTransform(rotationAngle: angle * .pi/180))
        imageLayer.opacity = Float(mix)
        oldImageLayer.isHidden = mix >= 1
        if let previous, mix < 1 {
            oldImageLayer.setAffineTransform(.identity)
            oldImageLayer.frame = rect(for: previous)
            oldImageLayer.setAffineTransform(CGAffineTransform(rotationAngle: angle * .pi/180))
            oldImageLayer.opacity = Float(1-mix)
        }
        bubbleLayer.isHidden = bubble.isEmpty; tailLayer.isHidden = bubble.isEmpty
        if !bubble.isEmpty {
            let font = NSFont.systemFont(ofSize: 12.5, weight: .medium)
            let size = (bubble as NSString).size(withAttributes: [.font: font])
            let w = min(bounds.width-12, size.width+24)
            let box = NSRect(x: (bounds.width-w)/2, y: min(bounds.height-39, r.maxY+12), width: w, height: 31)
            bubbleLayer.frame = box
            textLayer.contentsScale = window?.backingScaleFactor ?? 2
            if textLayer.string as? String != bubble { textLayer.string = bubble }
            textLayer.frame = NSRect(x: 6, y: 7, width: box.width-12, height: 18)
            let tail = CGMutablePath(); tail.move(to: NSPoint(x: box.midX-5, y: box.minY+1)); tail.addLine(to: NSPoint(x: box.midX, y: box.minY-5)); tail.addLine(to: NSPoint(x: box.midX+5, y: box.minY+1)); tail.closeSubpath()
            tailLayer.path = tail
        }
        CATransaction.commit()
    }

    override func mouseDown(with event: NSEvent) {
        if event.modifierFlags.contains(.control) { rightMouseDown(with: event); return }
        pressed = true; dragging = false
        dragStart = window?.convertPoint(toScreen: event.locationInWindow) ?? NSEvent.mouseLocation
        originalOrigin = window?.frame.origin ?? .zero
    }
    override func mouseDragged(with event: NSEvent) {
        guard pressed, let window else { return }
        let mouse = window.convertPoint(toScreen: event.locationInWindow)
        if !dragging && hypot(mouse.x-dragStart.x, mouse.y-dragStart.y) > 3 {
            dragging = true; controller?.beginDrag(character)
        }
        if dragging {
            window.setFrameOrigin(NSPoint(x: originalOrigin.x+mouse.x-dragStart.x, y: originalOrigin.y+mouse.y-dragStart.y))
        }
    }
    override func mouseUp(with event: NSEvent) {
        guard pressed else { return }; pressed = false
        if dragging { dragging = false; controller?.endDrag(character) }
        else { controller?.touched(character) }
    }
    override func rightMouseDown(with event: NSEvent) {
        guard let menu = controller?.contextMenu(character) else { return }
        NSMenu.popUpContextMenu(menu, with: event, for: self)
    }
}

final class Pet {
    let character: Character
    let panel: PetPanel
    let view: PetView
    var behavior: Behavior
    init(_ character: Character, controller: AppController, now: Double) {
        self.character = character
        self.behavior = Behavior(character: character, now: now)
        view = PetView(character: character, controller: controller)
        panel = makePanel(view)
    }
}

func makePanel(_ view: NSView) -> PetPanel {
    let panel = PetPanel(contentRect: NSRect(x: 0, y: 0, width: 250, height: 295), styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: false)
    panel.isOpaque = false; panel.backgroundColor = .clear; panel.hasShadow = false
    panel.level = .floating; panel.hidesOnDeactivate = false
    panel.isMovableByWindowBackground = false; panel.isReleasedWhenClosed = false
    panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .ignoresCycle]
    panel.contentView = view
    return panel
}

final class AppController: NSObject, NSApplicationDelegate, NSMenuDelegate {
    let sprites: Sprites
    let defaults: UserDefaults
    var pets: [Pet] = []
    var pairPanel: PetPanel!
    var pairView: PetView!
    var pair: Together?
    var pairUntil: Double = 0
    var nextPair: Double = 0
    var nextPairIndex = 0
    var approaching: Together?
    var approachAt: Double = 0
    var approachFrom: [NSPoint] = []
    var approachTo: [NSPoint] = []
    var timer: Timer?
    var status: NSStatusItem!
    var scale: CGFloat = 1
    var quiet = false
    var paused = false
    var hidden = false
    var mode = "both"
    var pausedAt: Double = 0
    var motionTime: Double = 0
    var lastTick: Double = 0
    var reduced = false
    var didSleep = false
    var trackingMenu = false
    var now: Double { ProcessInfo.processInfo.systemUptime }

    init(sprites: Sprites, defaults: UserDefaults = .standard) {
        self.sprites = sprites; self.defaults = defaults
        super.init()
        let saved = defaults.double(forKey: "size")
        scale = [0.8, 1.0, 1.25].contains(saved) ? saved : 1
        quiet = defaults.bool(forKey: "quiet")
        let savedMode = defaults.string(forKey: "mode") ?? "both"
        mode = ["both", "girl", "boy"].contains(savedMode) ? savedMode : "both"
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.accessory)
        pets = Character.allCases.map { Pet($0, controller: self, now: now) }
        pairView = PetView(character: nil, controller: self); pairPanel = makePanel(pairView)
        resizePanels(); restorePositions()
        for pet in pets {
            pet.behavior.set(.wave, now: now, duration: 3.5, text: pet.character == .girl ? "我来陪你啦。" : "右键找我玩，拖动搬个家。")
        }
        status = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        status.button?.image = NSImage(systemSymbolName: "person.2.fill", accessibilityDescription: "桌边的你们")
        status.button?.toolTip = "桌边的你们 · 点击管理小伙伴"
        status.menu = NSMenu(); status.menu?.delegate = self
        rebuildMenu(); applyVisibility()
        lastTick = now; nextPair = now+65
        reduced = NSWorkspace.shared.accessibilityDisplayShouldReduceMotion
        let center = NSWorkspace.shared.notificationCenter
        center.addObserver(self, selector: #selector(screenDidSleep), name: NSWorkspace.screensDidSleepNotification, object: nil)
        center.addObserver(self, selector: #selector(screenDidWake), name: NSWorkspace.screensDidWakeNotification, object: nil)
        center.addObserver(self, selector: #selector(accessibilityChanged), name: NSWorkspace.accessibilityDisplayOptionsDidChangeNotification, object: nil)
        NotificationCenter.default.addObserver(self, selector: #selector(screensChanged), name: NSApplication.didChangeScreenParametersNotification, object: nil)
        scheduleTimer()
        tick()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { false }
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        hidden = false; resetPositions(); applyVisibility(); return true
    }
    func applicationWillTerminate(_ notification: Notification) { savePositions(); timer?.invalidate() }

    @objc func tick() {
        let time = now
        let dt = min(0.1, max(0, time-lastTick)); lastTick = time
        guard !didSleep && !trackingMenu else { return }
        if !paused && !hidden { motionTime += dt }
        let hour = Calendar.current.component(.hour, from: Date())
        if let scene = approaching, !paused && !hidden {
            let progress = min(1, max(0, (time-approachAt-0.35)/0.65))
            let ease = 1-pow(1-progress, 3)
            for (i, pet) in pets.enumerated() {
                pet.panel.setFrameOrigin(NSPoint(x: approachFrom[i].x+(approachTo[i].x-approachFrom[i].x)*ease,
                                                y: approachFrom[i].y+(approachTo[i].y-approachFrom[i].y)*ease))
            }
            if progress >= 1 { approaching = nil; presentPair(scene) }
        }
        if !paused && !hidden && pair != nil && time >= pairUntil && !pairView.dragging { finishPair() }
        for pet in pets {
            if !paused && !hidden && pair == nil { pet.behavior.tick(now: time, hour: hour, quiet: quiet) }
            let view = pet.view
            view.now = paused ? pausedAt : time; view.motionTime = motionTime
            view.scale = scale; view.quiet = quiet; view.frozen = paused; view.reduced = reduced
            view.pose = pet.behavior.pose
            let pose = pet.behavior.displayedPose(now: view.now)
            view.show(sprites.characters[pet.character]![pose.rawValue], key: pose.rawValue)
            view.bubble = quiet || view.now >= pet.behavior.bubbleUntil ? "" : pet.behavior.bubble
            updatePointer(panel: pet.panel, view: view)
            view.setAccessibilityValue(pose.title(for: pet.character))
            if pet.panel.isVisible && !hidden && !paused { view.render() }
        }
        if let pair {
            pairView.now = paused ? pausedAt : time; pairView.motionTime = motionTime
            pairView.scale = scale; pairView.quiet = quiet; pairView.frozen = paused; pairView.reduced = reduced
            pairView.pose = pair == .heart ? .love : .idle
            pairView.show(sprites.together[pair.rawValue], key: pair.rawValue)
            pairView.bubble = quiet || time > pairUntil-6 ? "" : pair.message
            updatePointer(panel: pairPanel, view: pairView)
            pairView.setAccessibilityValue(pair.title)
            if !hidden && !paused { pairView.render() }
        }
        if !paused && !hidden && !quiet && mode == "both" && pair == nil && approaching == nil && time >= nextPair {
            nextPair = time + 75
            let a = pets[0], b = pets[1]
            if !a.view.dragging && !b.view.dragging && abs(a.panel.frame.midX-b.panel.frame.midX) < 300*scale && abs(a.panel.frame.minY-b.panel.frame.minY) < 65*scale {
                startPair(Together.allCases[nextPairIndex % 4]); nextPairIndex += 1
            }
        }
    }

    func updatePointer(panel: PetPanel, view: PetView) {
        guard panel.isVisible else { return }
        let mouse = NSEvent.mouseLocation
        let local = NSPoint(x: mouse.x-panel.frame.minX, y: mouse.y-panel.frame.minY)
        panel.ignoresMouseEvents = !view.dragging && !view.opaqueAt(local)
        let near = abs(local.x-view.bounds.midX) < 180 && local.y > 0 && local.y < 300
        let desired = near && !quiet && !paused && !reduced && !view.dragging && view.pose != .sleep ? max(-2, min(2, (local.x-view.bounds.midX)/55)) : 0
        view.lean += (desired-view.lean)*0.12
    }

    func touched(_ character: Character?) {
        approaching = nil
        if paused { setPaused(false) }
        if let character, let pet = pets.first(where: { $0.character == character }) { pet.behavior.touch(now: now) }
        else { finishPair(); for pet in pets { pet.behavior.set(.love, now: now, duration: 3, text: "也喜欢你。") } }
        nextPair = now+65; tick()
    }

    func beginDrag(_ character: Character?) {
        approaching = nil
        if paused { setPaused(false) }
        if let character, let pet = pets.first(where: { $0.character == character }) { pet.behavior.lift(now: now) }
        nextPair = now+70
    }
    func endDrag(_ character: Character?) {
        if let character, let pet = pets.first(where: { $0.character == character }) {
            clamp(pet.panel); pet.behavior.land(now: now); pet.view.landingAt = now
        } else { clamp(pairPanel); pairView.landingAt = now }
        savePositions()
    }

    func startPair(_ scene: Together) {
        guard mode == "both", !hidden, !pets.contains(where: { $0.view.dragging }) else { return }
        if paused { setPaused(false) }
        if pair != nil { finishPair() }
        approaching = nil
        guard !reduced else { presentPair(scene); return }
        let a = pets[0].panel.frame, b = pets[1].panel.frame
        let center = screen(for: pets[0].panel).frame == screen(for: pets[1].panel).frame ? (a.midX+b.midX)/2 : a.midX
        approachFrom = pets.map { $0.panel.frame.origin }
        approachTo = pets.enumerated().map { index, pet in
            NSPoint(x: center+(index == 0 ? -94 : 94)*scale-pet.panel.frame.width/2, y: a.minY)
        }
        for pet in pets { pet.behavior.set(.wave, now: now, duration: 2) }
        approachAt = now; approaching = scene; nextPair = now+75
        applyVisibility()
    }

    func presentPair(_ scene: Together) {
        let a = pets[0].panel.frame, b = pets[1].panel.frame
        let sameScreen = screen(for: pets[0].panel).frame == screen(for: pets[1].panel).frame
        let center = sameScreen ? (a.midX+b.midX)/2 : a.midX
        pairPanel.setFrameOrigin(NSPoint(x: center-pairPanel.frame.width/2, y: sameScreen ? min(a.minY,b.minY) : a.minY))
        clamp(pairPanel); pair = scene; pairUntil = now+11; pairView.previous = nil
        applyVisibility()
    }

    func finishPair() {
        approaching = nil
        guard pair != nil else { return }
        let frame = pairPanel.frame
        pair = nil; pairPanel.orderOut(nil)
        for (index, pet) in pets.enumerated() {
            pet.panel.setFrameOrigin(NSPoint(x: frame.midX + (index == 0 ? -108 : 108)*scale - pet.panel.frame.width/2, y: frame.minY))
            clamp(pet.panel)
            pet.behavior.set(.idle, now: now, duration: 0)
        }
        nextPair = now+75; applyVisibility(); savePositions()
    }

    func screen(for panel: NSWindow) -> NSScreen {
        let center = NSPoint(x: panel.frame.midX, y: panel.frame.midY)
        return NSScreen.screens.first(where: { $0.frame.contains(center) }) ?? NSScreen.main ?? NSScreen.screens[0]
    }
    func clamp(_ panel: NSWindow) {
        let a = screen(for: panel).visibleFrame
        let p = clampedOrigin(Point2(x: panel.frame.minX, y: panel.frame.minY), width: panel.frame.width, height: panel.frame.height, area: Area(x: a.minX, y: a.minY, width: a.width, height: a.height))
        panel.setFrameOrigin(NSPoint(x: p.x, y: p.y))
    }
    func resizePanels() {
        for pet in pets { pet.panel.setContentSize(NSSize(width: 250*scale, height: 290*scale)) }
        pairPanel?.setContentSize(NSSize(width: 465*scale, height: 300*scale))
    }
    func resetPositions() {
        if pair != nil { finishPair() }
        let screen = NSScreen.screens.first(where: { $0.frame.contains(NSEvent.mouseLocation) }) ?? NSScreen.main!
        let area = screen.visibleFrame
        for (index, pet) in pets.enumerated() {
            pet.panel.setFrameOrigin(NSPoint(x: area.maxX-(index == 0 ? 485 : 260)*scale-22, y: area.minY+16))
            clamp(pet.panel)
        }
        savePositions()
    }
    func restorePositions() {
        let saved = Character.allCases.map { defaults.array(forKey: "position.\($0.rawValue)") as? [Double] }
        resetPositions()
        for (i, pet) in pets.enumerated() {
            if let p = saved[i], p.count == 2, p.allSatisfy({ $0.isFinite }) {
                pet.panel.setFrameOrigin(NSPoint(x: p[0], y: p[1])); clamp(pet.panel)
            }
        }
    }
    func savePositions() {
        guard pair == nil else { return }
        for pet in pets { defaults.set([pet.panel.frame.minX, pet.panel.frame.minY], forKey: "position.\(pet.character.rawValue)") }
    }
    func applyVisibility() {
        for pet in pets {
            if !hidden && pair == nil && (mode == "both" || mode == pet.character.rawValue) { pet.panel.orderFrontRegardless() }
            else { pet.panel.orderOut(nil) }
        }
        if !hidden && pair != nil { pairPanel.orderFrontRegardless() } else { pairPanel.orderOut(nil) }
    }
    func setPaused(_ value: Bool) {
        if value == paused { return }
        if value { pausedAt = now }
        else {
            let elapsed = now-pausedAt
            for pet in pets { pet.behavior.shiftTime(by: elapsed); pet.view.changedAt += elapsed; pet.view.landingAt += elapsed }
            pairUntil += elapsed; nextPair += elapsed; approachAt += elapsed; pairView.changedAt += elapsed
        }
        paused = value
        scheduleTimer()
    }
    func scheduleTimer() {
        timer?.invalidate()
        guard !didSleep else { return }
        let interval: Double = paused || hidden ? 0.2 : (quiet || reduced ? 0.1 : 1/30)
        timer = Timer(timeInterval: interval, target: self, selector: #selector(tick), userInfo: nil, repeats: true)
        timer?.tolerance = interval*0.15
        RunLoop.main.add(timer!, forMode: .common)
    }
    @objc func screenDidSleep() { didSleep = true; lastTick = now; timer?.invalidate() }
    @objc func screenDidWake() { didSleep = false; lastTick = now; screensChanged(); scheduleTimer() }
    @objc func accessibilityChanged() { reduced = NSWorkspace.shared.accessibilityDisplayShouldReduceMotion; scheduleTimer() }
    @objc func screensChanged() {
        if pair != nil { clamp(pairPanel) }
        for pet in pets { clamp(pet.panel) }; savePositions()
    }

    func item(_ title: String, _ command: String, checked: Bool = false) -> NSMenuItem {
        let item = NSMenuItem(title: title, action: #selector(command(_:)), keyEquivalent: "")
        item.target = self; item.representedObject = command; item.state = checked ? .on : .off
        return item
    }
    func actionMenu(_ character: Character) -> NSMenu {
        let menu = NSMenu()
        for pose in [Pose.wave, .busy, .snack, .sleep, .love, .grumpy] {
            menu.addItem(item(pose.title(for: character), "pose.\(character.rawValue).\(pose.rawValue)"))
        }
        return menu
    }
    func pairMenu() -> NSMenu {
        let menu = NSMenu()
        for scene in Together.allCases { menu.addItem(item(scene.title, "pair.\(scene.rawValue)")) }
        return menu
    }
    func submenu(_ title: String, _ child: NSMenu, to parent: NSMenu) {
        let row = NSMenuItem(title: title, action: nil, keyEquivalent: ""); row.submenu = child; parent.addItem(row)
    }
    func contextMenu(_ character: Character?) -> NSMenu {
        let menu = character.map(actionMenu) ?? pairMenu()
        menu.delegate = self
        menu.addItem(.separator())
        if mode == "both" { submenu("和对方一起…", pairMenu(), to: menu) }
        menu.addItem(item(quiet ? "恢复活泼模式" : "安静陪伴", "quiet", checked: quiet))
        menu.addItem(item(paused ? "继续活动" : "暂停动作", "pause", checked: paused))
        menu.addItem(item("回到桌面右下角", "reset"))
        menu.addItem(item("暂时藏起来", "hide"))
        menu.addItem(item("退出桌边的你们", "quit"))
        return menu
    }
    func menuWillOpen(_ menu: NSMenu) {
        trackingMenu = true
        if menu === status.menu { rebuildMenu() }
    }
    func menuDidClose(_ menu: NSMenu) {
        trackingMenu = false
        nextPair = max(nextPair, now+15)
        if pair != nil { pairUntil = max(pairUntil, now+3) }
    }
    func rebuildMenu() {
        guard let menu = status?.menu else { return }; menu.removeAllItems()
        let title = NSMenuItem(title: "桌边的你们", action: nil, keyEquivalent: ""); title.isEnabled = false; menu.addItem(title)
        menu.addItem(item(hidden ? "叫他们出来" : "暂时藏起来", "hide"))
        menu.addItem(item(paused ? "继续活动" : "暂停动作", "pause", checked: paused))
        menu.addItem(item("安静陪伴", "quiet", checked: quiet))
        menu.addItem(.separator())
        let who = NSMenu()
        for (name, value) in [("两个人一起", "both"), ("只留红衣女孩", "girl"), ("只留白衣男孩", "boy")] { who.addItem(item(name, "mode.\(value)", checked: mode == value)) }
        submenu("谁来陪我", who, to: menu)
        let size = NSMenu()
        for (name, value) in [("小小只 · 80%", 0.8), ("刚刚好 · 100%", 1.0), ("大一点 · 125%", 1.25)] { size.addItem(item(name, "size.\(value)", checked: abs(scale-value)<0.01)) }
        submenu("人物大小", size, to: menu)
        for character in Character.allCases { submenu(character.name, actionMenu(character), to: menu) }
        submenu("两个人的时光", pairMenu(), to: menu)
        menu.addItem(item("回到桌面右下角", "reset"))
        menu.addItem(.separator())
        menu.addItem(item("怎么玩", "help"))
        let quit = item("退出桌边的你们", "quit"); quit.keyEquivalent = "q"; menu.addItem(quit)
    }

    @objc func command(_ sender: NSMenuItem) {
        guard let command = sender.representedObject as? String else { return }
        if command.hasPrefix("size."), let value = Double(command.dropFirst(5)) {
            scale = value; defaults.set(value, forKey: "size"); resizePanels(); screensChanged()
        } else if command.hasPrefix("mode.") {
            finishPair(); mode = String(command.dropFirst(5)); defaults.set(mode, forKey: "mode"); hidden = false; applyVisibility()
        } else if command.hasPrefix("pair."), let raw = Int(command.dropFirst(5)), let scene = Together(rawValue: raw) {
            hidden = false; mode = "both"; defaults.set(mode, forKey: "mode"); startPair(scene)
        } else if command.hasPrefix("pose.") {
            let parts = command.split(separator: ".")
            if parts.count == 3, let character = Character(rawValue: String(parts[1])), let raw = Int(parts[2]), let pose = Pose(rawValue: raw), let pet = pets.first(where: { $0.character == character }) {
                finishPair(); hidden = false; if mode != "both" { mode = character.rawValue; defaults.set(mode, forKey: "mode") }
                setPaused(false); pet.behavior.set(pose, now: now, duration: pose == .sleep ? 40 : (pose == .busy || pose == .snack ? 24 : 4))
                nextPair = now+65; applyVisibility()
            }
        } else {
            switch command {
            case "quiet": quiet.toggle(); defaults.set(quiet, forKey: "quiet"); nextPair = now+75
            case "pause": setPaused(!paused)
            case "hide": hidden.toggle(); applyVisibility()
            case "reset": hidden = false; resetPositions(); applyVisibility()
            case "help": showHelp()
            case "quit": NSApp.terminate(nil)
            default: break
            }
        }
        scheduleTimer()
        tick()
    }
    func showHelp() {
        let alert = NSAlert()
        alert.messageText = "很高兴，住进你的桌面。"
        alert.informativeText = "点一下，和我们打招呼。连续点几下，会有小脾气。\n\n按住人物拖动，给我们换个位置。右键选择读书、吃饭、比心或小睡。\n\n把两个人放近一些，偶尔会一起看书、拍拍头或抱一下。也可以从菜单直接选择。\n\n菜单栏的双人图标可以调整大小、单人显示、安静陪伴和暂停。藏起来后，也从这里叫我们回来。\n\n安静模式减少动作和主动打扰；系统“减少动态效果”会关闭呼吸、摇摆与淡入淡出。应用不读取屏幕、键盘或工作内容。"
        alert.addButton(withTitle: "知道啦")
        NSApp.activate(ignoringOtherApps: true); alert.runModal()
    }
}

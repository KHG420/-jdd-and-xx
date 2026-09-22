import AppKit

var assertions = 0
func check(_ condition: @autoclosure () -> Bool, _ message: String) {
    assertions += 1
    if !condition() { fputs("FAIL: \(message)\n", stderr); exit(1) }
}

var girl = Behavior(character: .girl, now: 100)
girl.set(.sleep, now: 100, duration: 20)
girl.touch(now: 101)
check(girl.pose == .blink, "sleep wakes gradually")
girl.tick(now: 101.8, hour: 12, quiet: false)
check(girl.pose == .wave, "wake completes with a wave")
girl.tick(now: 105, hour: 12, quiet: false)
check(girl.pose == .idle, "wave settles")
for i in 0..<4 { girl.touch(now: 106 + Double(i)*0.2) }
check(girl.pose == .grumpy, "repeated taps cause protest")
girl.tick(now: 110, hour: 12, quiet: false)
check(girl.pose == .idle, "protest recovers")
girl.lift(now: 111); girl.tick(now: 400, hour: 12, quiet: false)
check(girl.held && girl.pose == .lifted, "held pose cannot time out")
girl.land(now: 401); girl.tick(now: 401.5, hour: 12, quiet: false)
check(!girl.held && girl.pose == .idle, "landing recovers")
girl.set(.busy, now: 402, duration: 20); girl.touch(now: 403)
check(girl.pose == .idle, "puts book down before wave")
girl.tick(now: 403.4, hour: 12, quiet: false)
check(girl.pose == .wave, "busy interaction completes")
girl.shiftTime(by: 1000)
girl.tick(now: 1404, hour: 12, quiet: false)
check(girl.pose == .wave, "pause retains remaining response duration")
var boy = Behavior(character: .boy, now: 0)
boy.tick(now: 20, hour: 1, quiet: false)
check(boy.pose == .sleep, "nighttime starts sleep")
var calm = Behavior(character: .girl, now: 0)
for t in stride(from: 0.0, through: 600, by: 1) {
    calm.tick(now: t, hour: 12, quiet: true)
    check(![Pose.wave, .love, .grumpy].contains(calm.pose), "quiet does not initiate attention-seeking poses")
}
let p = clampedOrigin(Point2(x: -100, y: 999), width: 250, height: 290, area: Area(x: 0, y: 0, width: 1200, height: 800))
check(p.x == 0 && p.y == 510, "clamps full panel")
let negativeScreen = clampedOrigin(Point2(x: -1700, y: 30), width: 250, height: 290, area: Area(x: -1920, y: 0, width: 1920, height: 1080))
check(negativeScreen.x == -1700, "preserves negative monitor coordinates")

let app = NSApplication.shared
app.setActivationPolicy(.accessory)
let sprites = try Sprites(directory: URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("Assets"))
let all = Character.allCases.flatMap { sprites.characters[$0]! } + sprites.together
check(all.count == 22, "22 production poses load")
for sprite in all {
    check(sprite.alpha.contains(0), "transparent pixels exist")
    check(sprite.alpha.contains(255), "opaque character remains")
    check(sprite.width > 150 && sprite.height > 150, "no truncated empty cell")
    check(!sprite.contains(u: -0.1, v: 0.5), "outside sprite never intercepts click")
}

let suite = "local.aq.desktop-companions.tests.\(ProcessInfo.processInfo.processIdentifier)"
let defaults = UserDefaults(suiteName: suite)!
defer { defaults.removePersistentDomain(forName: suite) }
let controller = AppController(sprites: sprites, defaults: defaults)
controller.applicationDidFinishLaunching(Notification(name: NSApplication.didFinishLaunchingNotification))
controller.timer?.invalidate()
controller.reduced = true
check(controller.pets.count == 2 && controller.pets.allSatisfy { $0.panel.isVisible }, "two transparent native panels launch")
for pet in controller.pets {
    check(!pet.panel.isOpaque && !pet.panel.hasShadow && !pet.panel.canBecomeKey, "nonactivating transparent panels")
    check(!pet.view.opaqueAt(NSPoint(x: 0, y: 0)), "empty panel corner passes through")
}
func command(_ action: String) { controller.command(controller.item("test", action)) }
command("pair.3")
check(controller.pair == .hug && controller.pairPanel.isVisible, "hug enters shared scene")
check(controller.pets.allSatisfy { !$0.panel.isVisible }, "no duplicate characters during scene")
controller.finishPair()
check(controller.pair == nil && controller.pets.allSatisfy { $0.panel.isVisible }, "pair returns to individuals")
command("mode.girl")
check(controller.pets[0].panel.isVisible && !controller.pets[1].panel.isVisible, "single girl mode")
command("mode.boy")
check(!controller.pets[0].panel.isVisible && controller.pets[1].panel.isVisible, "single boy mode")
command("size.1.25")
check(abs(controller.pets[0].panel.frame.width - 312.5) <= 0.5, "resize applies to window with native pixel rounding")
command("quiet"); check(controller.quiet && defaults.bool(forKey: "quiet"), "quiet persists")
command("pause"); check(controller.paused, "pause applies")
command("pause"); check(!controller.paused, "resume applies")
command("hide"); check(controller.hidden && controller.pets.allSatisfy { !$0.panel.isVisible }, "hide removes windows")
command("hide"); check(!controller.hidden && controller.pets[1].panel.isVisible, "show respects selected mode")
command("pose.girl.4")
check(controller.mode == "girl" && controller.pets[0].behavior.pose == .snack, "menu action makes selected character visible")
command("pair.0"); command("hide"); command("hide")
check(controller.pairPanel.isVisible && controller.pets.allSatisfy { !$0.panel.isVisible }, "shared scene survives hide/show without duplicates")
controller.finishPair(); command("size.1.0")
for scene in Together.allCases { controller.startPair(scene); check(controller.pair == scene, "all paired scenes accessible"); controller.finishPair() }
controller.savePositions()
let saved = defaults.array(forKey: "position.girl") as! [Double]
controller.pets[0].panel.setFrameOrigin(NSPoint(x: -99999, y: -99999))
controller.restorePositions()
check(abs(controller.pets[0].panel.frame.minX-saved[0]) < 1, "saved positions restore")
controller.reduced = false
controller.startPair(.heart)
check(controller.approaching == .heart && controller.pair == nil, "pair notices partner before approaching")
controller.approachAt -= 2
controller.menuWillOpen(controller.contextMenu(.girl))
controller.tick()
check(controller.pair == nil && controller.approaching == .heart, "open menu holds autonomous transitions")
controller.menuDidClose(controller.contextMenu(.girl))
controller.tick()
check(controller.pair == .heart && controller.approaching == nil, "approach completes shared scene")
controller.finishPair()
controller.startPair(.read); controller.touched(.girl)
check(controller.approaching == nil, "click interrupts approach")
let dragged = controller.pets[0]
let origin = dragged.panel.frame.origin
func mouse(_ type: NSEvent.EventType, _ point: NSPoint) -> NSEvent {
    NSEvent.mouseEvent(with: type, location: point, modifierFlags: [], timestamp: controller.now,
                      windowNumber: dragged.panel.windowNumber, context: nil, eventNumber: 0, clickCount: 1, pressure: 1)!
}
dragged.view.mouseDown(with: mouse(.leftMouseDown, NSPoint(x: 120, y: 120)))
dragged.view.mouseDragged(with: mouse(.leftMouseDragged, NSPoint(x: 80, y: 170)))
check(dragged.view.dragging && dragged.behavior.held, "native drag enters lifted pose")
check(dragged.panel.frame.minX == origin.x-40 && dragged.panel.frame.minY == origin.y+50, "drag uses event coordinates, not global cursor")
dragged.view.mouseUp(with: mouse(.leftMouseUp, NSPoint(x: 80, y: 170)))
check(!dragged.view.dragging && !dragged.behavior.held, "native mouseUp lands character")
controller.setPaused(true)
check(controller.timer!.timeInterval == 0.2, "pause lowers polling and stops redraw")
controller.screenDidSleep()
check(!controller.timer!.isValid, "screen sleep stops timer")
controller.timer?.invalidate()
for pet in controller.pets { pet.panel.orderOut(nil) }; controller.pairPanel.orderOut(nil)

// Render actual keyed production sprites over light and dark surfaces, no screenshot access.
let canvas = NSImage(size: NSSize(width: 1440, height: 1150))
canvas.lockFocus()
NSColor(calibratedRed: 0.97, green: 0.95, blue: 0.91, alpha: 1).setFill()
NSRect(x: 0, y: 0, width: 1440, height: 1150).fill()
let titleAttrs: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 30, weight: .semibold), .foregroundColor: NSColor(calibratedRed: 0.25, green: 0.19, blue: 0.15, alpha: 1)]
("桌边的你们" as NSString).draw(at: NSPoint(x: 42, y: 1086), withAttributes: titleAttrs)
let small: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 14), .foregroundColor: NSColor.darkGray]
("两个人，慢慢住进你的日常。  /  实际应用角色素材" as NSString).draw(at: NSPoint(x: 44, y: 1060), withAttributes: small)
for (row, character) in Character.allCases.enumerated() {
    for (col, pose) in Pose.allCases.enumerated() {
        let sprite = sprites.characters[character]![pose.rawValue]
        let factor = 140 / sprite.referenceWidth
        let w = CGFloat(sprite.width)*factor, h = CGFloat(sprite.height)*factor
        let x = 20 + CGFloat(col)*157, y = 836-CGFloat(row)*223
        sprite.image.draw(in: NSRect(x: x+(150-w)/2, y: y, width: w, height: h))
        (pose.title(for: character) as NSString).draw(at: NSPoint(x: x+15, y: y-30), withAttributes: small)
    }
}
NSColor(calibratedRed: 0.13, green: 0.17, blue: 0.16, alpha: 1).setFill()
NSRect(x: 0, y: 0, width: 1440, height: 540).fill()
let light: [NSAttributedString.Key: Any] = [.font: NSFont.systemFont(ofSize: 18, weight: .medium), .foregroundColor: NSColor(calibratedRed: 0.95, green: 0.91, blue: 0.81, alpha: 1)]
("靠近一点，一起过小日子。" as NSString).draw(at: NSPoint(x: 44, y: 487), withAttributes: light)
for scene in Together.allCases {
    let sprite = sprites.together[scene.rawValue], x = CGFloat(scene.rawValue)*350+26
    let factor = 176 / sprite.referenceWidth
    let w = CGFloat(sprite.width)*factor, h = CGFloat(sprite.height)*factor
    sprite.image.draw(in: NSRect(x: x+(330-w)/2, y: 220, width: w, height: h))
    (scene.title as NSString).draw(at: NSPoint(x: x+115, y: 175), withAttributes: light)
}
("点一下，回应你。拖起来，换个家。忙的时候，我们安静陪着。" as NSString).draw(at: NSPoint(x: 44, y: 57), withAttributes: light)
canvas.unlockFocus()
let bitmap = NSBitmapImageRep(data: canvas.tiffRepresentation!)!
try bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: "build/角色动作预览.png"))
print("PASS: \(assertions) assertions; 22 keyed sprites; native menus, windows, paired scenes, persistence. Preview: build/角色动作预览.png")

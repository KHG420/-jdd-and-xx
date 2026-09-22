import Foundation

enum Character: String, CaseIterable {
    case girl, boy
    var name: String { self == .girl ? "红衣女孩" : "白衣男孩" }
}

enum Pose: Int, CaseIterable {
    case idle, blink, wave, busy, snack, sleep, lifted, grumpy, love
    func title(for character: Character) -> String {
        switch self {
        case .idle: return "陪在你身边"
        case .blink: return "眨眨眼"
        case .wave: return "打个招呼"
        case .busy: return character == .girl ? "翻一会儿书" : "认真敲电脑"
        case .snack: return character == .girl ? "喝口热茶" : "吃一口饭"
        case .sleep: return "打个小盹"
        case .lifted: return "被提起来了"
        case .grumpy: return "闹一点小脾气"
        case .love: return "送你一颗心"
        }
    }
}

enum Together: Int, CaseIterable {
    case read, heart, comfort, hug
    var title: String { ["一起看书", "一起比心", "拍拍头", "抱一抱"][rawValue] }
    var message: String { ["这一页，我们一起看。", "这颗心，送给你。", "累了就靠一会儿。", "抱一下，就好啦。"][rawValue] }
}

/// Absolute deadlines are shifted on resume, so pausing never consumes a response.
struct Behavior {
    let character: Character
    private(set) var pose: Pose = .idle
    private(set) var entered: Double = 0
    private(set) var deadline: Double = 0
    private(set) var bubble = ""
    private(set) var bubbleUntil: Double = 0
    private(set) var lastTouch: Double = -100
    private(set) var taps = 0
    private(set) var held = false
    private var pending: Pose?
    private var nextBlink: Double = 4
    private var blinkUntil: Double = 0
    private var nextActivity: Double = 12
    private var activityIndex = 0

    init(character: Character, now: Double) {
        self.character = character
        entered = now
        nextActivity = now + (character == .girl ? 12 : 19)
        nextBlink = now + (character == .girl ? 3.7 : 5.1)
    }

    mutating func set(_ value: Pose, now: Double, duration: Double, text: String = "") {
        pose = value; entered = now; deadline = now + duration
        pending = nil
        if !text.isEmpty { bubble = text; bubbleUntil = now + min(duration, 3.5) }
    }

    mutating func touch(now: Double) {
        taps = now - lastTouch < 2 ? taps + 1 : 1
        lastTouch = now
        if taps >= 4 {
            set(.grumpy, now: now, duration: 3, text: character == .girl ? "再戳，要叉腰啦。" : "脑袋要被戳扁啦！")
        } else if pose == .sleep {
            set(.blink, now: now, duration: 0.7, text: "嗯…你叫我？")
            pending = .wave
        } else if pose == .busy || pose == .snack {
            set(.idle, now: now, duration: 0.35)
            pending = .wave
        } else {
            set(taps == 3 ? .love : .wave, now: now, duration: 2.6,
                text: character == .girl ? "我在呀。" : "嘿，我陪着你呢！")
        }
    }

    mutating func lift(now: Double) {
        held = true
        set(.lifted, now: now, duration: .infinity, text: character == .girl ? "慢一点呀～" : "起飞咯！")
    }

    mutating func land(now: Double) {
        held = false
        set(.blink, now: now, duration: 0.4, text: "稳稳落地。")
        pending = .idle
        nextActivity = now + 15
    }

    mutating func tick(now: Double, hour: Int, quiet: Bool) {
        guard !held else { return }
        if (pose != .idle || pending != nil) && now >= deadline {
            let next = pending ?? .idle
            set(next, now: now, duration: next == .wave ? 2.5 : 0)
            nextActivity = now + (quiet ? 45 : 18)
        }
        if pose == .idle && now >= nextActivity {
            let choices: [Pose] = quiet ? [.busy, .sleep, .busy] : [.busy, .snack, .idle, .sleep, .busy, .love]
            let next = (hour >= 23 || hour < 7) ? Pose.sleep : choices[activityIndex % choices.count]
            activityIndex += 1
            if next != .idle { set(next, now: now, duration: next == .sleep ? 38 : (next == .love ? 3 : 24)) }
            nextActivity = now + (quiet ? 50 : 30)
        }
        if now >= nextBlink {
            blinkUntil = now + 0.16
            nextBlink = now + (character == .girl ? 4.6 : 5.7)
        }
    }

    func displayedPose(now: Double) -> Pose {
        pose == .idle && now < blinkUntil ? .blink : pose
    }

    mutating func shiftTime(by seconds: Double) {
        entered += seconds; deadline += seconds; bubbleUntil += seconds
        lastTouch += seconds; nextBlink += seconds; blinkUntil += seconds; nextActivity += seconds
    }
}

struct Point2 { var x: Double; var y: Double }
struct Area { var x: Double; var y: Double; var width: Double; var height: Double }

func clampedOrigin(_ point: Point2, width: Double, height: Double, area: Area) -> Point2 {
    Point2(x: min(max(point.x, area.x), max(area.x, area.x + area.width - width)),
           y: min(max(point.y, area.y), max(area.y, area.y + area.height - height)))
}

import AppKit

let application = NSApplication.shared
let resources = Bundle.main.resourceURL ?? URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("Assets")
let directory = FileManager.default.fileExists(atPath: resources.appendingPathComponent("girl-atlas.png").path)
    ? resources : URL(fileURLWithPath: FileManager.default.currentDirectoryPath).appendingPathComponent("Assets")
do {
    let sprites = try Sprites(directory: directory)
    let controller = AppController(sprites: sprites)
    application.delegate = controller
    withExtendedLifetime(controller) { application.run() }
} catch {
    let alert = NSAlert(); alert.messageText = "小伙伴暂时没能出门"
    alert.informativeText = "\(error)\n请重新运行项目里的「启动桌宠.command」来重建完整应用。"
    alert.addButton(withTitle: "知道了"); alert.runModal()
    exit(1)
}

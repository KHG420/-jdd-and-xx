import AppKit
import ImageIO

struct Sprite {
    let image: NSImage
    let cgImage: CGImage
    let width: Int
    let height: Int
    let alpha: [UInt8]
    let referenceWidth: CGFloat

    func contains(u: CGFloat, v: CGFloat) -> Bool {
        guard u >= 0, u < 1, v >= 0, v < 1 else { return false }
        let x = min(width - 1, Int(u * CGFloat(width)))
        let y = min(height - 1, Int((1 - v) * CGFloat(height)))
        return alpha[y * width + x] > 30
    }
}

enum SpriteError: Error, CustomStringConvertible {
    case invalid(String)
    var description: String { if case let .invalid(s) = self { return s }; return "素材错误" }
}

final class Sprites {
    var characters: [Character: [Sprite]] = [:]
    var together: [Sprite] = []

    init(directory: URL) throws {
        for character in Character.allCases {
            // The generated poses slightly cross mathematical thirds; use inspected gutters.
            let cuts: [CGFloat] = character == .girl ? [0, 432.0 / 1254, 814.0 / 1254, 1] : [0, 435.0 / 1254, 830.0 / 1254, 1]
            characters[character] = try Self.load(directory.appendingPathComponent("\(character.rawValue)-atlas.png"), columns: 3, cuts: cuts, paired: false)
        }
        together = try Self.load(directory.appendingPathComponent("together-atlas.png"), columns: 2, cuts: [0, 0.5, 1], paired: true)
    }

    static func load(_ url: URL, columns: Int, cuts: [CGFloat], paired: Bool) throws -> [Sprite] {
        guard let source = CGImageSourceCreateWithURL(url as CFURL, nil),
              let raw = CGImageSourceCreateImageAtIndex(source, 0, nil) else {
            throw SpriteError.invalid("无法读取角色素材：\(url.lastPathComponent)")
        }
        var result: [Sprite] = []
        for row in 0..<(cuts.count - 1) {
            for column in 0..<columns {
                let x0 = Int(CGFloat(raw.width * column) / CGFloat(columns))
                let x1 = Int(CGFloat(raw.width * (column + 1)) / CGFloat(columns))
                let y0 = Int(cuts[row] * CGFloat(raw.height))
                let y1 = Int(cuts[row + 1] * CGFloat(raw.height))
                guard let tile = raw.cropping(to: CGRect(x: x0, y: y0, width: x1-x0, height: y1-y0)) else {
                    throw SpriteError.invalid("角色图集布局不完整")
                }
                result.append(try key(tile, referenceWidth: CGFloat(raw.width) / CGFloat(columns) / (paired ? 2 : 1)))
            }
        }
        return result
    }

    /// Real-time chroma key: preserve warm paper/skin and dark grain, remove green spill.
    static func key(_ source: CGImage, referenceWidth: CGFloat) throws -> Sprite {
        let width = source.width, height = source.height
        var pixels = [UInt8](repeating: 0, count: width * height * 4)
        let color = CGColorSpaceCreateDeviceRGB()
        let info = CGImageAlphaInfo.premultipliedLast.rawValue | CGBitmapInfo.byteOrder32Big.rawValue
        let drawn = pixels.withUnsafeMutableBytes { bytes -> Bool in
            guard let ctx = CGContext(data: bytes.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: color, bitmapInfo: info) else { return false }
            ctx.draw(source, in: CGRect(x: 0, y: 0, width: width, height: height))
            return true
        }
        guard drawn else { throw SpriteError.invalid("无法创建角色纹理") }
        var minX = width, maxX = 0, minY = height, maxY = 0
        for y in 0..<height {
            for x in 0..<width {
                let i = (y * width + x) * 4
                let r = Double(pixels[i]), g = Double(pixels[i+1]), b = Double(pixels[i+2])
                let dominance = g - max(r, b)
                let opacity = 1 - min(1, max(0, (dominance - 12) / 80))
                let a = UInt8((opacity * 255).rounded())
                pixels[i] = UInt8(r * opacity)
                pixels[i+1] = UInt8((dominance > 12 ? min(g, max(r, b)) : g) * opacity)
                pixels[i+2] = UInt8(b * opacity)
                pixels[i+3] = a
                if a > 30 { minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y) }
            }
        }
        guard minX < maxX, minY < maxY else { throw SpriteError.invalid("角色纹理是空白的") }
        // Fixed cell scale, trimmed padding: sitting poses do not acquire oversized heads.
        minX = max(0, minX - 2); minY = max(0, minY - 2)
        maxX = min(width - 1, maxX + 2); maxY = min(height - 1, maxY + 2)
        let w = maxX - minX + 1, h = maxY - minY + 1
        var trimmed = [UInt8](repeating: 0, count: w * h * 4)
        var alpha = [UInt8](repeating: 0, count: w * h)
        for y in 0..<h { for x in 0..<w {
            let from = ((y + minY) * width + x + minX) * 4, to = (y * w + x) * 4
            for c in 0..<4 { trimmed[to+c] = pixels[from+c] }
            alpha[y*w+x] = pixels[from+3]
        } }
        guard let provider = CGDataProvider(data: Data(trimmed) as CFData),
              let cg = CGImage(width: w, height: h, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: w*4, space: color, bitmapInfo: CGBitmapInfo(rawValue: info), provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent) else {
            throw SpriteError.invalid("无法解码角色透明纹理")
        }
        return Sprite(image: NSImage(cgImage: cg, size: NSSize(width: w, height: h)), cgImage: cg, width: w, height: h, alpha: alpha, referenceWidth: referenceWidth)
    }
}

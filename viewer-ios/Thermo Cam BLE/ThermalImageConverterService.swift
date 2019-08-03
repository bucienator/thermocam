//
//  ThermalImageConverterService.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 06. 10..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import Foundation
import UIKit
import Metal

private struct PixelData {
    var a:UInt8 = 255
    var r:UInt8
    var g:UInt8
    var b:UInt8
}

private func CalculateColorGradient(_ colorMap: inout [PixelData], startIdx: Int, endIdx: Int, startColor: PixelData, endColor: PixelData)
{
    let stepCnt = endIdx-startIdx
    let diffA = (Float(endColor.a) - Float(startColor.a))/Float(stepCnt)
    let diffR = (Float(endColor.r) - Float(startColor.r))/Float(stepCnt)
    let diffG = (Float(endColor.g) - Float(startColor.g))/Float(stepCnt)
    let diffB = (Float(endColor.b) - Float(startColor.b))/Float(stepCnt)
    for i in 0..<stepCnt {
        colorMap[startIdx + i].a = UInt8(Float(startColor.a) + (diffA * Float(i)))
        colorMap[startIdx + i].r = UInt8(Float(startColor.r) + (diffR * Float(i)))
        colorMap[startIdx + i].g = UInt8(Float(startColor.g) + (diffG * Float(i)))
        colorMap[startIdx + i].b = UInt8(Float(startColor.b) + (diffB * Float(i)))
    }
    
}

private class ColorScale {
    
    private var colorScale : [PixelData]
    
    init() {
        colorScale = [PixelData](repeating: PixelData(a:0, r:0, g:0, b:0), count: 1000)
        
        let black = PixelData(a: 255, r: 0, g: 0, b: 0)
        let blue = PixelData(a: 255, r: 0, g: 0, b: 255)
        let cyan = PixelData(a: 255, r: 0, g: 255, b: 255)
        let green = PixelData(a: 255, r: 0, g: 255, b: 0)
        let yellow = PixelData(a: 255, r: 255, g: 255, b: 0)
        let red = PixelData(a: 255, r: 255, g: 0, b: 0)
        let white = PixelData(a: 255, r: 255, g: 255, b: 255)
        
        CalculateColorGradient(&colorScale, startIdx: 0, endIdx: 200, startColor: black, endColor: blue)
        CalculateColorGradient(&colorScale, startIdx: 200, endIdx: 250, startColor: blue, endColor: cyan)
        CalculateColorGradient(&colorScale, startIdx: 250, endIdx: 300, startColor: cyan, endColor: green)
        CalculateColorGradient(&colorScale, startIdx: 300, endIdx: 350, startColor: green, endColor: yellow)
        CalculateColorGradient(&colorScale, startIdx: 350, endIdx: 400, startColor: yellow, endColor: red)
        CalculateColorGradient(&colorScale, startIdx: 400, endIdx: 1000, startColor: red, endColor: white)
    }
    
    func GetColorForTemperature(_ temp: Float) -> PixelData {
        let unboundIdx = Int(temp * 10)
        let index = unboundIdx < 0 ? 0 : unboundIdx >= 1000 ? 999 : unboundIdx
        
        return colorScale[index]
    }
}

private func imageFromARGB32Bitmap(pixels: [PixelData], width: Int, height: Int) -> CGImage {
    let bitsPerComponent: Int = 8
    let bitsPerPixel: Int = 32
    
    assert(pixels.count == Int(width * height))
    
    var data = pixels // Copy to mutable []
    let providerRef = CGDataProvider(
        data: NSData(bytes: &data, length: data.count * 4)
        )!
    
    let cgim = CGImage(
        width: width,
        height: height,
        bitsPerComponent: bitsPerComponent,
        bitsPerPixel: bitsPerPixel,
        bytesPerRow: width * 4,
        space: CGColorSpaceCreateDeviceRGB(),
        bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipFirst.rawValue),
        provider: providerRef,
        decode: nil,
        shouldInterpolate: true,
        intent: CGColorRenderingIntent.defaultIntent
    )
    return cgim!
}

protocol ThermalImageConverterServiceDelegate : class {
    func conversionComplete(_ image: CGImage)
}

class ThermalImageConverterService : MetalScalerDelegate {

    static let shared = ThermalImageConverterService()

    private let scaler : MetalScaler?
    
    private let colorScale = ColorScale()
    private var _targetSize = CGSize(width: 414, height: 414)
    var targetSize : CGSize {
        set {
            _targetSize = newValue
            scaler?.targetSize = newValue
        }
        get {
            return _targetSize
        }
    }
    
    init() {
        let device = MTLCreateSystemDefaultDevice()
        if device != nil {
            // Actually OpenCV's resize seems to be much faster than my Metal GPU scaler
            // bu probably I could tweak it to be more efficient.
            // Now it is 350 FPS to 80 FPS in release on iPhone 6s plus.
            scaler = MetalScaler(device!)
            scaler?.delegate = self
        } else {
            scaler = nil
        }
    }
    
    func startImageConversion(_ heatMap: [Float], completionHandler delegate: ThermalImageConverterServiceDelegate?)
    {
        
        if scaler != nil {
            // Metal solution
            scaler?.scale(heatMap, completionHandler: delegate)
        } else {
            DispatchQueue.global(qos: .userInitiated).async {
                [weak self] in
                self?.scaleWithOpenCV(heatMap, completionHandler: delegate)
            }
        }
    }
    
    private func scaleWithOpenCV(_ heatMap: [Float], completionHandler delegate: ThermalImageConverterServiceDelegate?) {
        
        let targetPixelCount = Int(_targetSize.width * _targetSize.height)
        
        var scaled = [Float](repeating: 0, count: targetPixelCount)
        
        // OpenCV solution
        heatMap.withUnsafeBufferPointer {
            (inputBuffer: UnsafeBufferPointer)->Void in
            scaled.withUnsafeMutableBufferPointer {
                ( buf: inout UnsafeMutableBufferPointer)->Void in
                ImageScaler.scaleImage(from: CGSize(width: 8, height: 8), andFromData: inputBuffer.baseAddress!, to: _targetSize, andToData: buf.baseAddress!)
            }
        }
        
        scalingComplete(scaled, to: _targetSize, completionHandler: delegate)
    }
    
    func scalingComplete(_ scaled: [Float], to size: CGSize, completionHandler delegate: ThermalImageConverterServiceDelegate?) {
        let targetPixelCount = Int(size.width) * Int(size.height)
        var imageData = [PixelData](repeating: PixelData(a:0, r:0, g:0, b:0), count: targetPixelCount)
        
        //let minTemp = heatMap.min()!
        //let maxTemp = heatMap.max()!
        
        for i in 0..<targetPixelCount {
            let temp = scaled[targetPixelCount-1-i]
            imageData[i] = colorScale.GetColorForTemperature((temp - 20.0) * 4.0)
        }
        
        let image = imageFromARGB32Bitmap(pixels: imageData, width: Int(size.width), height: Int(size.height))
        delegate?.conversionComplete(image)
    }
}



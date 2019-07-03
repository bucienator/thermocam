//
//  ThermalImageConverterService.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 06. 10..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import Foundation
import UIKit


public struct PixelData {
    var a:UInt8 = 255
    var r:UInt8
    var g:UInt8
    var b:UInt8
}

func CalculateColorGradient(_ colorMap: inout [PixelData], startIdx: Int, endIdx: Int, startColor: PixelData, endColor: PixelData)
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

class ColorScale {

    static let shared = ColorScale()
    
    var colorScale : [PixelData]
    
    private init() {
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


public func imageFromARGB32Bitmap(pixels: [PixelData], width: Int, height: Int) -> CGImage {
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


func ConvertThermalImageToCGImage(_ heatMap: [Float], scaleTo targetSize: CGSize) -> CGImage
{

    var imageData = [PixelData](repeating: PixelData(a:0, r:0, g:0, b:0), count: 64)
    
    //let minTemp = heatMap.min()!
    //let maxTemp = heatMap.max()!
    
    for i in 0..<64 {
        let temp = heatMap[63-i]
        imageData[i] = ColorScale.shared.GetColorForTemperature(temp)
    }
    
    return imageFromARGB32Bitmap(pixels: imageData, width: 8, height: 8)
}

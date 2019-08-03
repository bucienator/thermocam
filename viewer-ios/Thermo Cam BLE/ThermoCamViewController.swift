//
//  ViewController.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 05. 22..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import UIKit
import CoreBluetooth


class FrameRateCalculator {
    private var frameTimes: [Date]?
    private var first: Int
    private var next: Int
    private let steps: Int
    
    init(_ steps: Int) {
        self.steps = steps
        frameTimes = nil
        first = 0
        next = 0
    }
    
    func frameReady() -> Double {
        let now = Date()

        if frameTimes == nil {
            frameTimes = [Date](repeating: now, count: steps)
            first = 0
            next = 1
            return 0.0
        } else {
            if(first == next) {
                first = (first + 1) % steps
            }
            frameTimes![next] = now
            
            let numOfFrames = (next - first + steps) % steps // number of intervals between the frames
            let firstTime = frameTimes![first]
            let elapsedTime = now.timeIntervalSinceReferenceDate - firstTime.timeIntervalSinceReferenceDate
            next = (next + 1) % steps
            
            return Double(numOfFrames) / Double(elapsedTime)
        }
    }
}


class ThermoCamViewController: UIViewController, BLEHandlerDelegate, ThermalImageConverterServiceDelegate {
    

    @IBOutlet weak var thermalView: UIImageView!
    @IBOutlet weak var frameRateLabel: UILabel!
    var thermalViewSize = CGSize(width: 100, height: 100)
    var frameRate = FrameRateCalculator(100)
    
    
    
    override func viewDidLoad() {
        super.viewDidLoad()
        BLEHandler.shared.delegate = self
        
        print("\(ImageScaler.openCVVersionString())")
    }

    func didReceiveNewImage(_ image: [Float]) {
        // this is called on a background thread
        
        ThermalImageConverterService.shared.startImageConversion(image, completionHandler: self)
        
    }

    func conversionComplete(_ image: CGImage) {
        let imageToShow = UIImage(cgImage: image)
        
        DispatchQueue.main.async {
            [weak self] in
            self?.thermalView.image = imageToShow
            let fps = self?.frameRate.frameReady()
            self?.frameRateLabel.text = "\(fps ?? 0)"
            DispatchQueue.global(qos: .background).async() {
                [weak self] in
                BLEHandler.shared.demoImage()
            }
        }
    }

    override func viewDidLayoutSubviews() {
        let newImageSize = thermalView.bounds.size
        if thermalViewSize != newImageSize {
            thermalViewSize = newImageSize
            ThermalImageConverterService.shared.targetSize = thermalViewSize
        }
        //thermalViewSize.width *= UIScreen.main.scale
        //thermalViewSize.height *= UIScreen.main.scale
    }
}


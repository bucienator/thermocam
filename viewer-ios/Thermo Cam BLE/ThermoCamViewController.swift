//
//  ViewController.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 05. 22..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import UIKit
import CoreBluetooth


class ThermoCamViewController: UIViewController, BLEHandlerDelegate {

    @IBOutlet weak var thermalView: UIImageView!
    override func viewDidLoad() {
        super.viewDidLoad()
        BLEHandler.shared.delegate = self
    }

    func didReceiveNewImage(_ image: [Float]) {
        // this is called on a background thread
        
        let img2 = ConvertThermalImageToCGImage(image, scaleTo: CGSize(width: 8, height: 8))
        let imageToShow = UIImage(cgImage: img2)
        
        DispatchQueue.main.async {
            [weak self] in
            self?.thermalView.image = imageToShow
        }
    }
    

}


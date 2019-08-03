//
//  BLEThermal.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 06. 08..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import Foundation
import UIKit
import CoreBluetooth


protocol BLEHandlerDelegate : class {
    func didReceiveNewImage(_ image: [Float])
}

class BLEHandler : NSObject {
    weak var delegate : BLEHandlerDelegate?
    
    let thermoCamServiceUUID = CBUUID(string: "97B8FCA2-45A8-478C-9E85-CC852AF2E950")
    let thermoCamCharacteristicUUID = CBUUID(string: "52E66CFC-9DD2-4932-8E81-7EAF2C6E2C53")
    
    var centralManager : CBCentralManager!
    var thermoCamPeripheral : CBPeripheral?

    private override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: DispatchQueue.global(qos: .background))
    }
    
    static let shared = BLEHandler()
    
    
    // MARK: - demo tempreature map
    let demoTemperatureBase = Float(drand48() * 10.0 + 20.0)
    var demoTemperatures = [Float](repeating: 25.0, count: 64)
    let maxDeviance = Float(5.0)
    
    func updateDemoTemperatures() {
        // update the demo temperature map.
        // Each pixel either getw warmer or colder, depending on it's deviation form the
        // base temperature
        for i in 0..<64 {
            let currentTemp = demoTemperatures[i];
            let currentDeviance = currentTemp - demoTemperatureBase
            if(Float(drand48()) * maxDeviance > fabsf(currentDeviance) ) {
                // deviance to be increased
                demoTemperatures[i] = demoTemperatureBase + currentDeviance * 1.1
            } else {
                // deviance to be decreased
                demoTemperatures[i] = demoTemperatureBase + currentDeviance * 0.9
            }
        }
    }
    
    func demoImage() {

        updateDemoTemperatures()
        
        self.delegate?.didReceiveNewImage(demoTemperatures)
        
        // reschedule a next demo image
        // resceduling afer frame is processed...
        /*DispatchQueue.global(qos: .background).asyncAfter(deadline: .now() + 1) {
            [weak self] in
            self?.demoImage()
        }*/
    }
    
}

extension BLEHandler : CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .unknown:
            print("central.state is .unknown")
        case .resetting:
            print("central.state is .resetting")
        case .unsupported:
            print("central.state is .unsupported")
            
            // dispatch a demo event
            DispatchQueue.global(qos: .background).asyncAfter(deadline: .now()+1) {
                [weak self] in
                self?.demoImage()
            }

        case .unauthorized:
            print("central.state is .unauthorized")
        case .poweredOff:
            print("central.state is .poweredOff")
        case .poweredOn:
            print("central.state is .poweredOn")
//            central.scanForPeripherals(withServices: [thermoCamServiceUUID])
            // dispatch a demo event
            DispatchQueue.global(qos: .background).asyncAfter(deadline: .now()+1) {
                [weak self] in
                self?.demoImage()
            }

        @unknown default:
            print("central.state is something else")
            
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print(peripheral)
        centralManager.stopScan()
        thermoCamPeripheral = peripheral
        centralManager.connect(peripheral)
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("connected")
        guard let thermoCamPeripheral = thermoCamPeripheral else {
            print("lost peripheral")
            return
        }
        
        thermoCamPeripheral.delegate = self
        thermoCamPeripheral.discoverServices([thermoCamServiceUUID])
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("disconnected form peripheral")
    }
}

extension BLEHandler : CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else {
            print("no services found")
            return
        }
        
        for service in services {
            peripheral.discoverCharacteristics(nil, for: service)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else {
            print ("lost characteristics")
            return
        }
        
        for characteristic in characteristics {
            print (characteristic)
            peripheral.setNotifyValue(true, for: characteristic)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        switch characteristic.uuid {
        case thermoCamCharacteristicUUID:
            guard let data = characteristic.value else {
                print("data update failed")
                return
            }
            
            let intData = [UInt8](data)
            var temperatures = [Float](repeating: 0, count: 64)
            for i in 0..<64 {
                let pixel = UInt16(intData[i * 2]) | (UInt16(intData[i * 2 + 1]) << 8)
                let sign = pixel & 0x0400 != 0
                if (!sign) {
                    // positive
                    temperatures[i] = Float(pixel & 0x07ff) / 4;
                }
                else {
                    // negative
                    temperatures[i] = -Float(((~pixel) & 0x07ff) + 1) / 4;
                }
            }
            
            //print("Received \(intData.count)")
            
            self.delegate?.didReceiveNewImage(temperatures)
            
        default:
            print("Unexpected data received")
        }
    }
    
}

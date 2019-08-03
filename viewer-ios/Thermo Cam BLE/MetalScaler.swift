//
//  MetalScaler.swift
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 07. 20..
//  Copyright © 2019. Metacortex. All rights reserved.
//

import Foundation
import Metal
import Dispatch

protocol MetalScalerDelegate : class
{
    func scalingComplete(_ scaled: [Float], to size: CGSize, completionHandler delegate: ThermalImageConverterServiceDelegate?)
}

class MetalScaler
{
    private let device: MTLDevice

    // The compute pipeline generated from the compute kernel in the .metal shader file.
    private let scaleFunctionPSO: MTLComputePipelineState
    private let calculateCoeffsFunctionPSO: MTLComputePipelineState
    private let normalizeCoeffsFunctionPSO: MTLComputePipelineState
    
    // The command queue used to pass commands to the device.
    private let commandQueue: MTLCommandQueue
    
    // Buffers to hold data.
    private let inputBuffer: MTLBuffer
    private let targetSizeBuffer: MTLBuffer
    private var resultBuffer: MTLBuffer?
    private var coeffBuffer: MTLBuffer?
    
    // target size
    private var _targetSize = CGSize(width: 8, height: 8)
    var targetSize: CGSize {
        get {
            return _targetSize
        }
        set {
            workQueue.async {
                [weak self] in
                self?.changeTargetSize(newValue)
            }
        }
    }
    
    // queue
    private var workQueue: DispatchQueue
    
    var delegate: MetalScalerDelegate?
    
    init?(_ device: MTLDevice) {
        self.device = device
        
        // Load the shader files with a .metal file extension in the project
        let defaultLibrary = device.makeDefaultLibrary()
        let scaleFunction = defaultLibrary?.makeFunction(name: "scale")
        let calculateCoeffsFunction = defaultLibrary?.makeFunction(name: "init_coeff")
        let normalizeCoeffsFunction = defaultLibrary?.makeFunction(name: "normalize_coeff")
        
        // Create a compute pipeline state object.
        do {
            scaleFunctionPSO = try device.makeComputePipelineState(function: scaleFunction!)
            calculateCoeffsFunctionPSO = try device.makeComputePipelineState(function: calculateCoeffsFunction!)
            normalizeCoeffsFunctionPSO = try device.makeComputePipelineState(function: normalizeCoeffsFunction!)
        } catch {
            return nil
        }
        commandQueue = device.makeCommandQueue()!
        
        inputBuffer = device.makeBuffer(length: 8*8*4, options: .storageModeShared)!
        targetSizeBuffer = device.makeBuffer(length: 1*4, options: .storageModeShared)!
        
        workQueue = DispatchQueue(label: "MetalScaler", qos: .userInitiated)
    }
    
    
    private func changeTargetSize(_ size: CGSize)
    {
        resultBuffer = device.makeBuffer(length: Int(size.width) * Int(size.height) * 4, options: .storageModeShared)
        coeffBuffer = device.makeBuffer(length: Int(size.width) * Int(size.height) * 64 * 4, options: .storageModePrivate)
        
        let buffer = targetSizeBuffer.contents()
        buffer.storeBytes(of: Float(size.width), as: Float.self)
        
        _targetSize = size
        initializeCoeff(size)
        normalizeCoeff(size)
    }
    
    private func initializeCoeff(_ size: CGSize) {
        // Create a command buffer to hold commands.
        let commandBuffer = commandQueue.makeCommandBuffer()
        
        // Start a compute pass.
        let computeEncoder = commandBuffer?.makeComputeCommandEncoder()
        
        // Encode the pipeline state object and its parameters.
        computeEncoder?.setComputePipelineState(calculateCoeffsFunctionPSO)
        computeEncoder?.setBuffer(coeffBuffer, offset: 0, index: 0)
        computeEncoder?.setBuffer(targetSizeBuffer, offset: 0, index: 1)
        
        // Calculate a threadgroup size
        let coeffElements = Int(size.width) * Int(size.height)
        let maxTotalThreads = calculateCoeffsFunctionPSO.maxTotalThreadsPerThreadgroup
        assert(maxTotalThreads % 64 == 0)
        let threadGroupHeight = maxTotalThreads / 64
        let threadgroupCount = (coeffElements + threadGroupHeight - 1) / threadGroupHeight
        let threadsPerThreadgroup = MTLSizeMake(64, threadGroupHeight, 1)
        let threadgroupsPerGrid = MTLSizeMake(1, threadgroupCount, 1)
        
        // Encode the compute command.
        computeEncoder?.dispatchThreadgroups(threadgroupsPerGrid, threadsPerThreadgroup: threadsPerThreadgroup)
        
        // End the compute pass.
        computeEncoder?.endEncoding()
        
        // Execute the command.
        commandBuffer?.commit()
        
        // Normally, you want to do other work in your app while the GPU is running,
        // but in this example, the code simply blocks until the calculation is complete.
        commandBuffer?.waitUntilCompleted()

    }
    
    private func normalizeCoeff(_ size: CGSize) {
        // Create a command buffer to hold commands.
        let commandBuffer = commandQueue.makeCommandBuffer()
        
        // Start a compute pass.
        let computeEncoder = commandBuffer?.makeComputeCommandEncoder()
        
        // Encode the pipeline state object and its parameters.
        computeEncoder?.setComputePipelineState(normalizeCoeffsFunctionPSO)
        computeEncoder?.setBuffer(coeffBuffer, offset: 0, index: 0)
        computeEncoder?.setBuffer(targetSizeBuffer, offset: 0, index: 1)
        
        // Calculate a threadgroup size
        let coeffElements = Int(size.width) * Int(size.height)
        let maxTotalThreads = normalizeCoeffsFunctionPSO.maxTotalThreadsPerThreadgroup
        let threadGroupWidth = maxTotalThreads
        let threadgroupCount = (coeffElements + threadGroupWidth - 1) / threadGroupWidth
        let threadsPerThreadgroup = MTLSizeMake(threadGroupWidth, 1, 1)
        let threadgroupsPerGrid = MTLSizeMake(threadgroupCount, 1, 1)
        
        // Encode the compute command.
        computeEncoder?.dispatchThreadgroups(threadgroupsPerGrid, threadsPerThreadgroup: threadsPerThreadgroup)
        
        // End the compute pass.
        computeEncoder?.endEncoding()
        
        // Execute the command.
        commandBuffer?.commit()
        
        // Normally, you want to do other work in your app while the GPU is running,
        // but in this example, the code simply blocks until the calculation is complete.
        commandBuffer?.waitUntilCompleted()
    }
    
    func scale(_ heatMap: [Float], completionHandler delegate: ThermalImageConverterServiceDelegate?) {
        workQueue.async {
            [weak self] in
            self?.executeScale(heatMap, completionHandler: delegate)
        }
    }
    
    private func executeScale(_ heatMap: [Float], completionHandler ch: ThermalImageConverterServiceDelegate?) {
        
        heatMap.withUnsafeBufferPointer() {
            (heatMapBuffer: UnsafeBufferPointer)->Void in
            let buffer = inputBuffer.contents()
            buffer.initializeMemory(as: Float.self, from: heatMapBuffer.baseAddress!, count: 64)
        }
        
        // Create a command buffer to hold commands.
        let commandBuffer = commandQueue.makeCommandBuffer()
        
        // Start a compute pass.
        let computeEncoder = commandBuffer?.makeComputeCommandEncoder()
        
        // Encode the pipeline state object and its parameters.
        computeEncoder?.setComputePipelineState(scaleFunctionPSO)
        computeEncoder?.setBuffer(inputBuffer, offset: 0, index: 0)
        computeEncoder?.setBuffer(resultBuffer, offset: 0, index: 1)
        computeEncoder?.setBuffer(coeffBuffer, offset: 0, index: 2)
        computeEncoder?.setBuffer(targetSizeBuffer, offset: 0, index: 3)

        // Calculate a threadgroup size
        let coeffElements = Int(_targetSize.width) * Int(_targetSize.height)
        let maxTotalThreads = scaleFunctionPSO.maxTotalThreadsPerThreadgroup
        let threadGroupWidth = maxTotalThreads
        let threadgroupCount = (coeffElements + threadGroupWidth - 1) / threadGroupWidth
        let threadsPerThreadgroup = MTLSizeMake(threadGroupWidth, 1, 1)
        let threadgroupsPerGrid = MTLSizeMake(threadgroupCount, 1, 1)
        
        // Encode the compute command.
        computeEncoder?.dispatchThreadgroups(threadgroupsPerGrid, threadsPerThreadgroup: threadsPerThreadgroup)
        
        // End the compute pass.
        computeEncoder?.endEncoding()
        
        // Execute the command.
        commandBuffer?.commit()
        
        // Normally, you want to do other work in your app while the GPU is running,
        // but in this example, the code simply blocks until the calculation is complete.
        commandBuffer?.waitUntilCompleted()

        let targetPixelCount = Int(_targetSize.width * _targetSize.height)
        var scaled = [Float](repeating: 0, count: targetPixelCount)

        scaled.withUnsafeMutableBufferPointer() {
            (scaledBuffer: inout UnsafeMutableBufferPointer)->Void in
            let buffer = resultBuffer!.contents()
            let floatBuffer = buffer.bindMemory(to: Float.self, capacity: Int(_targetSize.width) * Int(_targetSize.height))
            scaledBuffer.baseAddress?.assign(from: floatBuffer, count: Int(_targetSize.width) * Int(_targetSize.height))
        }
        
        delegate?.scalingComplete(scaled, to: _targetSize, completionHandler: ch)
        
    }
}

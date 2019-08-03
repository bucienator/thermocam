//
//  ImageScaler.m
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 07. 03..
//  Copyright © 2019. Metacortex. All rights reserved.
//

#import <opencv2/opencv.hpp>

#import "ImageScaler.h"

@implementation ImageScaler

+ (NSString *)openCVVersionString {
    
    return [NSString stringWithFormat:@"OpenCV Version %s",  CV_VERSION];
    
}


+ (void) scaleImageFromSize: (CGSize) inputSize andFromData: (float [_Nonnull]) inputData toSize: (CGSize) outputSize andToData: (float [_Nonnull]) outputData {
    
    cv::Mat input(inputSize.width, inputSize.height, CV_32F, inputData);
    cv::Mat output(outputSize.width, outputSize.height, CV_32F, outputData);
    
    cv::resize(input, output, output.size(), 0, 0, cv::INTER_CUBIC);
}


@end

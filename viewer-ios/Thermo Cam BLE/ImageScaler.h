//
//  ImageScaler.h
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 07. 03..
//  Copyright © 2019. Metacortex. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface ImageScaler : NSObject
+ (NSString *)openCVVersionString;

+ (void) scaleImageFromSize: (CGSize) inputSize andFromData: (const float [_Nonnull]) inputData toSize: (CGSize) outputSize andToData: (float [_Nonnull]) outputData;
@end

NS_ASSUME_NONNULL_END

//
//  scaler.metal
//  Thermo Cam BLE
//
//  Created by Bolla Péter on 2019. 07. 20..
//  Copyright © 2019. Metacortex. All rights reserved.
//

#include <metal_stdlib>
using namespace metal;


/*
    coeff is layd out rows after rows,
 
   0   1   2   3   4 ....  63
  64  65  66  67  68 .... 127
 128 129 130 131 132 .... 191
 
 index.x -> pixel in resulting image goes from 0 .. <w*h, which is runtime value
 index.y -> pixel in source image, goes form 0 .. <64, which is constant, as we always scale 8x8 image
 */
kernel void scale(device const float* input [[buffer(0)]],
                  device float* result [[buffer(1)]],
                  device const float* coeff [[buffer(2)]],
                  device const float* target_width_buf [[buffer(3)]], // single element
                  uint index [[thread_position_in_grid]])
{
    const uint target_width = uint(target_width_buf[0]);
    
    if(index >= target_width * target_width) {
        return; // last threadgroup includes thread off the buffer limit
    }
    
    result[index] = 0;
    for(int i = 0; i < 64; ++i) {
        result[index] += input[i] * coeff[index * 64 + i];
    }
}

static float sinc(const float x)
{
    return x == 0 ? 1 : (sin(x) / x);
}

static float normalized_sinc(const float x)
{
    return sinc(x * M_PI_F);
}

static float sinc3_weight(const float x)
{
    return fabs(x) >= 3 ? 0 : (normalized_sinc(x) * normalized_sinc(x / 3));
}

kernel void init_coeff(device float* coeff [[buffer(0)]],
                       device const float* target_width_buf [[buffer(1)]], // single element
                       uint2 index [[thread_position_in_grid]])
{
    const uint target_width = uint(target_width_buf[0]);
    
    if(index.y >= target_width * target_width) {
        return; // last threadgroup includes thread off the buffer limit
    }
    
    const float source_x = float(index.x % 8);
    const float source_y = float(index.x / 8);
    const float target_x = float(index.y % target_width);
    const float target_y = float(index.y / target_width);

    // Calculate float coordinates of target pixel in source coordinates.
    // Original image covers (-0.5 .. 7.5, with a sample point at each integer)
    // Target image should cover the same area, with evenly placed sample points
    const float scaled_pixel_size = 8.0f / target_width;
    const float scaled_range_start = -0.5f + scaled_pixel_size / 2.0f;
    
    const float scaled_target_x = scaled_range_start + scaled_pixel_size * target_x;
    const float scaled_target_y = scaled_range_start + scaled_pixel_size * target_y;

    const float diff_x = source_x - scaled_target_x;
    const float diff_y = source_y - scaled_target_y;
    
    coeff[index.y * 64 + index.x] = sinc3_weight(diff_x) * sinc3_weight(diff_y);
}

kernel void normalize_coeff(device float* coeff [[buffer(0)]],
                            device const float* target_width_buf [[buffer(1)]], // single element
                            uint index [[thread_position_in_grid]])
{
    const uint target_width = uint(target_width_buf[0]);
    
    if(index >= target_width * target_width) {
        return; // last threadgroup includes thread off the buffer limit
    }
    
    float accumulator = 0.0;
    for(int i = 0; i < 64; i++) {
        accumulator += coeff[index * 64 + i];
    }
    for(int i = 0; i < 64; i++) {
        coeff[index * 64 + i] /= accumulator;
        //coeff[index * 64 + i] = 1/64;
    }
}

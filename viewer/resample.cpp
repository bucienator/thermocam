#include "pch.h"
#include "resample.h"

static float sinc(const float x)
{
	return x == 0 ? 1 : (sin(x) / x);
}

static float normalized_sinc(const float x)
{
	return sinc(x * M_PI);
}

static float lanczos_weight(const float x)
{
	static const int window_size = 3;
	return std::abs(x) >= window_size ? 0 : (normalized_sinc(x) * normalized_sinc(x / window_size));
}

static float cached_lanczos_weight(const float x, const float y)
{
	static std::map<std::pair<float, float>, float> cached_values;
	const auto ret = cached_values.insert(std::make_pair(std::make_pair(x, y), 0.0f));
	if (ret.second) {
		ret.first->second = lanczos_weight(x) * lanczos_weight(y);
	}

	return ret.first->second;
}


std::vector<float> resampleThermalImage(const std::vector<float>& input, const int scaled_size)
{
	// The input is expected to be 8x8 pixels
	// It is scaled to scaled_size x scaled_size pixels
	assert(input.size() == 8 * 8);

	// calculate float coordinates of this pixel in the original image's scale.
	// Original image covers (-0.5 .. 7.5, with a sample point at each integer)
	// Target image should cover the same area, with evenly placed sample points
	const float scaled_pixel_size = 8.0f / scaled_size;
	const float scaled_range_start = -0.5f + scaled_pixel_size / 2.0f;

	std::vector<float> output(scaled_size * scaled_size, 0.0f);

	for (unsigned row = 0; row < scaled_size; ++row) {
		const float f_row = scaled_range_start + row * scaled_pixel_size;
		for (unsigned col = 0; col < scaled_size; ++col) {
			const float f_col = scaled_range_start + col * scaled_pixel_size;

			float accumulator = 0;
			float weight = 0;

			const int first_effective_row = static_cast<int>(floor(f_row)) - 2;
			const int last_effective_row = static_cast<int>(ceil(f_row)) + 2;
			const int first_effective_col = static_cast<int>(floor(f_col)) - 2;
			const int last_effective_col = static_cast<int>(ceil(f_col)) + 2;

			for (int source_row = first_effective_row; source_row <= last_effective_row; ++source_row) {
				const int effective_source_row = source_row < 0 ? 0 : (source_row > 7 ? 7 : source_row);
				for (int source_col = first_effective_col; source_col <= last_effective_col; ++source_col) {
					const int effective_source_col = source_col < 0 ? 0 : (source_col > 7 ? 7 : source_col);

					const float source_value = input[effective_source_row * 8 + effective_source_col];
					const float d_row = f_row - source_row;
					const float d_col = f_col - source_col;
					const float source_weight = cached_lanczos_weight(d_row, d_col);

					accumulator += source_value * source_weight;
					weight += source_weight;
				}
			}

			const float target_value = accumulator / weight;

			output[row * scaled_size + col] = target_value;
		}
	}

	return output;
}

import numpy as np
import math
from scipy.signal import convolve2d
from PIL import Image

# rough corresponding wavelengths for retinal cone sensitivities
LONG_WAVELENGTH = 700.0
MEDIUM_WAVELENGTH = 550.0
SHORT_WAVELENGTH = 450.0

WAVELENGTH_DEVIATION = 40.0

filepath = input("Please specify an image filepath: ").strip()

def gaussian_response(wavelength, mu, sigma):
    exponent = -((wavelength - mu) ** 2) / (2 * sigma ** 2)
    return math.exp(exponent)

def gaussian_contribution(rgb_array, wavelength):
    rgb = [
        rgb_array[..., 0], 
        rgb_array[..., 1], 
        rgb_array[..., 2]
    ]
    responses = [
        gaussian_response(wavelength, LONG_WAVELENGTH, WAVELENGTH_DEVIATION),
        gaussian_response(wavelength, MEDIUM_WAVELENGTH, WAVELENGTH_DEVIATION),
        gaussian_response(wavelength, SHORT_WAVELENGTH, WAVELENGTH_DEVIATION)
    ]
    contribution = sum(rgb[i] * responses[i] for i in range(3))
    return contribution

try:
    in_image = Image.open(filepath).convert("RGB")
    print("Image processing started...")

    # pre-normalized RGB
    rgb_array = np.asarray(in_image).astype(np.float32) / 255.0

    wavelength = SHORT_WAVELENGTH
    wavelength_delta = (LONG_WAVELENGTH - SHORT_WAVELENGTH) / 10.0

    accumulated_weights = np.zeros(rgb_array.shape[:2], dtype=np.float32)
    accumulated_count = 0

    while wavelength <= LONG_WAVELENGTH:
        contribution = gaussian_contribution(rgb_array, wavelength)
        kernel_size = int(2 * (WAVELENGTH_DEVIATION / wavelength_delta)) + 1
        kernel = np.outer(
            np.hanning(kernel_size),
            np.hanning(kernel_size)
        )
        kernel /= np.sum(kernel)

        contribution = convolve2d(contribution, kernel, mode='same', boundary='wrap')
        accumulated_weights[..., 0] += contribution * (wavelength == LONG_WAVELENGTH)
        accumulated_weights[..., 1] += contribution * (wavelength == MEDIUM_WAVELENGTH)
        accumulated_weights[..., 2] += contribution * (wavelength == SHORT_WAVELENGTH)

        wavelength += wavelength_delta
        accumulated_count += 1

    accumulated_weights /= accumulated_count

    basename = filepath.split('/')[-1].split('.')[0]
    out_file = f"{basename}_diffracted.png"
    out_image = Image.fromarray(accumulated_weights, mode='L')
    out_image.save(out_file)
    print(f"Image processing completed. Saved results to {out_file}...")


except FileNotFoundError:
    print(f"File not found: {filepath}")
    exit(1)
except Exception as e:
    print(f"Error opening image: {e}")
    exit(1)

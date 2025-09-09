import numpy as np
import math
import sys
from scipy.signal import fftconvolve
from scipy.ndimage import zoom
from PIL import Image

# rough corresponding wavelengths for retinal cone sensitivities
LONG_WAVELENGTH = 700.0
MEDIUM_WAVELENGTH = 550.0
SHORT_WAVELENGTH = 450.0

WAVELENGTHS = [LONG_WAVELENGTH, MEDIUM_WAVELENGTH, SHORT_WAVELENGTH]

REFENCE_WAVELENGTH = SHORT_WAVELENGTH

WAVELENGTH_RESOLUTION = 10.0
WAVELENGTH_DELTA = (LONG_WAVELENGTH - SHORT_WAVELENGTH) / WAVELENGTH_RESOLUTION

WAVELENGTH_DEVIATION = 40.0

DIFFRACTION_RASTER_SIZE = 512

# the following functions derive from the sRGB color space conversion formulae as described here
# https://entropymine.com/imageworsener/srgbformula/

def srgb_to_linear(c):
    if c <= 0.04045:
        return c / 12.92
    else:
        return ((c + 0.055) / 1.055) ** 2.4
    
def linear_to_srgb(c):
    if c <= 0.0031308:
        return c * 12.92
    else:
        return 1.055 * (c ** (1 / 2.4)) - 0.055
    

# point spread functions

def save_debug_image(array, path, logarithmic=False):
    data = array.astype(np.float64)
    if logarithmic:
        data = np.log10(data + 1e-10)
        data -= data.min()
        data /= data.max() if data.max() > 0 else 1
    else:
        lo, hi = np.percentile(data, [0.1, 99.9])
        if hi > lo:
            data = np.clip((data - lo) / (hi - lo), 0, 1)
        else:
            data[:] = 0
    image = (data * 255 + 0.5).astype(np.uint8)
    Image.fromarray(image, mode='L').save(path)

def polygonal_aperture(blades, rotation):
    raster_size = DIFFRACTION_RASTER_SIZE
    rotation = math.radians(rotation)
    x, y = np.linspace(-1, 1, raster_size), np.linspace(-1, 1, raster_size)
    xv, yv = np.meshgrid(x, y)
    mask = np.ones((raster_size, raster_size), dtype=bool)
    max = math.cos(math.pi / blades)
    for i in range(blades):
        theta = 2.0 * math.pi * i / blades + rotation
        mask &= (xv * math.cos(theta) + yv * math.sin(theta)) <= max
    return mask.astype(np.float32)

def compute_psf(aperture):
    # IMPORTANT: MUST ZERO OUT THE RESPONSE W/ SHIFT ELSE IT GETS OVER/UNDERFLOW ARTIFACTING
    fft = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(aperture)))
    psf = np.abs(fft) ** 2
    psf /= psf.sum()
    return psf

def reshape_psf(psf, target_shape):
    target_h, target_w = target_shape
    psf_h, psf_w = psf.shape

    if psf_h > target_h:
        start = (psf_h - target_h) // 2
        psf = psf[start:start + target_h, :]
    elif psf_h < target_h:
        pad_before = (target_h - psf_h) // 2
        pad_after = target_h - psf_h - pad_before
        psf = np.pad(psf, ((pad_before, pad_after), (0, 0)))

    if psf_w > target_w:
        start = (psf_w - target_w) // 2
        psf = psf[:, start:start + target_w]
    elif psf_w < target_w:
        pad_before = (target_w - psf_w) // 2
        pad_after = target_w - psf_w - pad_before
        psf = np.pad(psf, ((0, 0), (pad_before, pad_after)))

    return psf

def rescale_psf(psf, wavelength):
    scale_factor = wavelength / SHORT_WAVELENGTH
    psf_scaled = zoom(psf, scale_factor, order=1)
    psf_scaled /= psf_scaled.sum()
    return psf_scaled

def compute_diffraction(image, blades, rotation):
    aperture = polygonal_aperture(blades, rotation)
    save_debug_image(aperture, "diffracted_aperture.png")
    psf = compute_psf(aperture)

    lo, hi = np.percentile(psf, [0.1, 99.9])
    if hi > lo:
        psf = np.clip((psf - lo) / (hi - lo), 0, 1)
    else:
        psf[:] = 0

    save_debug_image(psf, "diffracted_psf.png", logarithmic=False)

    # sRGB: https://ninedegreesbelow.com/photography/srgb-luminance.html
    luminance = 0.2126 * image[..., 0] + 0.7152 * image[..., 1] + 0.0722 * image[..., 2]
    # proper relinearization with gamma
    luminance = np.pow(luminance, 1.5)

    save_debug_image(luminance, "diffracted_luminance.png")

    result = np.zeros_like(image)

    for channel in range(len(WAVELENGTHS)):
        channel_psf = rescale_psf(psf, WAVELENGTHS[channel])
        convolution = fftconvolve(image[..., channel] * luminance, channel_psf, mode='same')
        result[..., channel] = convolution

    max_psf = result.max()
    if max_psf > 0:
        result /= max_psf

    MULTIPLIER = 3.0
    out = image + MULTIPLIER * result
    return np.clip(out, 0, 1)


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 diffract.py <image_filepath>")
        exit(1)
    
    filepath = sys.argv[1].strip()
    blades = 7
    rotation = -math.radians(14 * 360 / blades)

    try:
        in_image = Image.open(filepath).convert("RGB")
        print("Image processing started...")
    except FileNotFoundError:
        print(f"File not found: {filepath}")
        exit(1)
    except Exception as e:
        print(f"File error: {e}")
        exit(1)

    # pre-normalized RGB
    rgb_array = np.asarray(in_image).astype(np.float32) / 255.0
    linear_rgb_array = np.vectorize(srgb_to_linear)(rgb_array)

    diffraction_pattern = compute_diffraction(linear_rgb_array, blades, rotation)
    diffraction_pattern = np.clip(diffraction_pattern, 0.0, 1.0)
    diffraction_srgb = np.vectorize(linear_to_srgb)(diffraction_pattern)

    out_array = (diffraction_srgb * 255.0 + 0.5).astype(np.uint8)

    basename = filepath.split('/')[-1].split('.')[0]
    out_file = f"{basename}_diffracted.png"
    Image.fromarray(out_array, mode='RGB').save(out_file)
    print(f"Image processing completed. Saved results to {out_file}...")

if __name__ == "__main__":
    main()
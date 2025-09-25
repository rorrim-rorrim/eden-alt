# User configuration

## Configuration directories

Eden will store configuration in the following directories:

- **Windows**: `%AppData%\Roaming`.
- **Android**: Data is stored internally.
- **Linux, macOS, FreeBSD, Solaris, OpenBSD**: `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`, `$XDG_CONFIG_HOME`.

If a `user` directory is present in the current working directory, that will override all global configuration directories and the emulator will use that instead.

# Enhancements

## Filters

Various graphical filters exist - each of them aimed at a specific target/image quality preset.

- **Nearest**: Provides no filtering - useful for debugging.
- **Bilinear**: Provides the hardware default filtering of the Tegra X1.
- **Bicubic**: Provides a bicubic interpolation using a Catmull-Rom (or hardware-accelerated) implementation.
- **Zero-Tangent, B-Spline, Mitchell**: Provides bicubic interpolation using the respective matrix weights. They're normally not hardware accelerated unless the device supports the `VK_QCOM_filter_cubic_weights` extension. The matrix weights are those matching [the specification itself](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkSamplerCubicWeightsCreateInfoQCOM).
- **Spline-1**: Bicubic interpolation (similar to Mitchell) but with a faster texel fetch method. Generally less blurry than bicubic.
- **Gaussian**: Whole-area blur, an applied gaussian blur is done to the entire frame.
- **Lanczos**: An implementation using `a = 3` (49 texel fetches). Provides sharper edges but blurrier artifacts.
- **ScaleForce**: Experimental texture upscale method, see [ScaleFish](https://github.com/BreadFish64/ScaleFish).
- **FSR**: Uses AMD FidelityFX Super Resolution to enhance image quality.
- **Area**: Area interpolation (high kernel count).
- **MMPX**: Nearest-neighbour filter aimed at providing higher pixel-art quality.

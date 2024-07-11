# gegl-HDR-ColorMapper
Chromaticity mapping algorithm for range compression or contrast adjustment.

I'm the reporter of https://gitlab.gnome.org/GNOME/gegl/-/issues/171 that ended up in a native blending-based saturation approach.
And I also published some code that lead Gimp to implement the Lightness Blend mode https://bugzilla.gnome.org/show_bug.cgi?id=753163.

*To make things clear: I'm not a native programmer - so my code looks ugly :-) - it should only serve as PoC.*

# Motivation
A lot of pictures look very desaturated when increasing image contrast. All receipes do not really help in a full extent: They adopt lightness/luminance and -contrast but to not map color correct from a color-intensity perspective.
- using GIMP LCH Lightness blend mode to adopt image contrast not even scales chroma at all. It controls lightness completely independent from chromaticity.
- even using GIMP Lightness blend mode - that I brought up - does not fully satisfy the chromaticity/saturation aspect - despite keeping saturation constant while scaling lightness. Perceived result fits much better, than the above mentioned pure LCH Lightness blend mode.

Examples:
This image shows, the large original image. The contrast-enhanced one, that looks desaturated and the one processed by this gegl-HDR-ColorMapper.
![ColorMapExample](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/7f5c92ee-cfe1-443c-b268-6d441895a48f)

Now the same with the HDR RangeCompressor Mantiuk 06. HDR itself looks faded from color perspective.
![ColorMapExample_Mantiuk](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/b9cf00a5-8798-4d45-8895-17105cb62cb2)

And now a more massive version: what I often do is, to create an grayscale image with c2g and use it as "exposure map" for the image (aka luminance layer mode in GIMP).
![ColorMapExample_c2g](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/a9b28d39-871f-4ca9-8917-83e94d848079)

# The Concept
## Theory
- Increasing image contrast looks like compressing / squeezing the light parts of an image and the dark ones more together. So not only the lightness of each pixel is adopted, **but squeezing means, that the density of color pigments (saturation) increases also**.
- or the other way around: increasing contrast stretches things on a smaller lightness range to an wider range. So basically that is "stretching". Stretching means, that the same amount of color pigments is distributed in a bigger area. That reduces color density (saturation). This might be an explanation, why contrast stretched images "lack" pigments (saturation). To compensate for this effect, just increase the number of pigments (chromaticity) by the same ratio like the size of the surface increases.

The proposed algo takes this model into account and does some scaling in linear RGB light. (Or, indeed it utilizes parts of the HSL(cie) color model, where L corresponds to CIE Y.)

The algo can cope with non-whitebalanced images and offers therefore an selector to define "physical white" - things that are perceived white under standard lighting conditions. It multiplies to neutral, does the changes in saturation, and afterward it puts the original tint back on the image.

**Also the method differs massively from just scaling overall image saturation. The math adopts each single pixel according to its appropriate new saturation. Areas with decreased contrast will be desaturated and chromaticity will be boosted in areas with increased contrast.**

Currently the operation is done as gegl-operation, but it could also be implemented als layer mode.

The "aux" input reads the "original" image with a consistent natural color. The layer where the gegl-op runs holds the desired luminance of the final image.

## Weaknesses
- contrast can sometimes only be "read out" (reverse engineered) from the image (by determining linear image gradient divided by luminance). Preference should always be to directly derive contrast changes. From changes in tone curve, for example by making the Chroma Channel dependent on Luminance based tone curve.
- the algo is not resilient by nature to image noise, that is indeed some kind of contrast (image gradient)
- handling noise is currently done by some experimental filters.

# Some tech details
I started and failed with the initial idea, that reducing contras to "0" will result in a luminance-constant image with neutral gray.

The idea is super for desaturating highlight or shadows in s-shaped contrast curves (like RGB filmic in Darktable does). But there was no way to realize luminance-constant coloured surfaces.

Thus I modified gegl:image-gradient that is used by the core of gegl HDR colormapper:

1) operate in linear light to stay in scene referred workflow as long as possible.
2) divide gradient by luminance (to make gradient independent from gegl:exposure. Global exposure changes image gradient but relative image gradient stays constant.)
3) represent image gradient ("delta") as a angle of gradient ("slope"). --> so the secant of gradient-angle represents something like "density" of image (projection of high contrasts to luminance-constant image represent high density ">> 1.0" and low contrasts are already luminance-constant with density "1.0")

So the chroma of a completely flattened image (luminance-constant) doesn't drop to zero, but gets only reduced according to density of "1.0" :-)

**This page is WIP!**

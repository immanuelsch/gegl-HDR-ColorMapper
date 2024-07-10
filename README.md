# gegl-HDR-ColorMapper
Chromaticity mapping algorithm for range compression or contrast adjustment.

I'm the reporter of https://gitlab.gnome.org/GNOME/gegl/-/issues/171 that ended up in a native blending-based (native) saturation approach.
And I also published some code that lead Gimp to implement the Lightness Blend mode https://bugzilla.gnome.org/show_bug.cgi?id=753163.
To make things clear: I'm not a programmer - so my code looks ugly.

# Motivation
A lot of pictures look very desaturated when increasing image contrast. All receipes do not really help in a full extent: They adopt lightness/luminance and -contrast but to not map color correct form a color-intensity perspective.
- using GIMP LCH Lightness blend mode to adopt image contrast
- even using GIMP Lightness blend mode - that I brought up - does not fully satisfy the chromaticity/saturation aspect - even it satisfies saturation receptioni much better, than the above mentioned.

Examples:
This image shows, the big original image. The contrast-enhanced one, that looks desaturated and the one processed by this gegl-HDR-ColorMapper.
![ColorMapExample](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/7f5c92ee-cfe1-443c-b268-6d441895a48f)

Now the same with the HDR RangeCompressor Mantiuk 06. HDR itself looks faded from color perspective.
![ColorMapExample_Mantiuk](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/b9cf00a5-8798-4d45-8895-17105cb62cb2)

And now a more massive version: what I often do is, to create an grayscale image with c2g and create this as "exposure map" for the Image (aka luminance layer mode in GIMP).
![ColorMapExample_c2g](https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/a9b28d39-871f-4ca9-8917-83e94d848079)

# The Concept
Theory:
- Increasing image contrast looks like compressing / squeezing the light parts of an image and the dark ones more together. So not only the lightness of each pixel is adopted, but squeezing means, that the density of color pigments (saturation) increases also.
- or the other way around: increasing contrast stretches things on a lower lightness scale to an wider range ranging from lower light up to brighter light. Stretching. Stretching means, that the same amount of color pigments is distributed in a bigger area. So color density (saturation) is reduced. Thats why contrast stretched images "lack" pigments (saturation). To compensate this, just increase the number of pigments (chromaticity) by the same scale like the area increases.

The proposed algo takes this model into account and does some scaling in linear RGB light. (Or indeed it utilizes parts of the HSL(cie) color model, where L corresponds to CIE Y.)
The algo consumes non-whitebalanced images and offers therefore an selector to select "physical white" - things that are perceived white under standard lighting conditions. It multiplies to neutral, does the changes in saturation, and afterward it puts the original tint back on the image.
Also the method differs massively from just scaling overall image saturation. The math adopts each single pixel according with its appropriate new saturation.
Currently the operation is done as gegl-operation, but it could also be implemented als layer mode.
The "aux" input reads the "original" image with an balanced natural color. The current layer reflects the desired luminance.

There are several Shortcomings:
- contrast can sometimes only be "read out" from the image (image gradient). Better would be to directly derive contrast changes. From changes in tone curve, for example.
- the algo is not resilient by nature to image noise, that is indeed some kind of contrast (image gradient)
- handling noise is currently done by some experimental filters.

This page is WIP.

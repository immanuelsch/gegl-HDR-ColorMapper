# gegl-HDR-ColorMapper
Chromaticity mapping algorithm for range compression or contrast adjustment.

I'm the reporter of https://gitlab.gnome.org/GNOME/gegl/-/issues/171 that ended up in a native blending-based saturation approach.
And I also published some code that lead Gimp to implement the Luminance Blend mode https://bugzilla.gnome.org/show_bug.cgi?id=753163.

*To make things clear: I'm not a native programmer - so my code looks ugly :-) - it should only serve as PoC.*

# Motivation
I do not really like (HS)Value-based tone-curves, because they considerabely lack accuracy (examples shown below in tech part). Instead I changed over to an luminance-based approach making luminance and color workflow independent:
1) edit the image tonality using the CIE Y Luminance representation - aka the b&w image / desaturated image / grayscale image.
2) after that, colorize that b&w image colorimetrically correct. 

So luminance-based tone curves are there to edit tonality separated from the color. This tone-adjustment activity is simply monochromatic. No need for RGB-channels. After (luminance-)tonality fits - there should not be a need to tune colors afterwards.

But unfortunately I haven't found an available solution. A lot of pictures look very desaturated when increasing image contrast on Y channel. All receipes do not really help in a full extent: They adopt lightness/luminance and -contrast but to not map color correct from a color-intensity perspective.
- using GIMP LCH Lightness blend mode to adopt image contrast not even scales chroma at all. It controls lightness completely independent from chromaticity.
- even using GIMP Luminance blend mode - that I brought up - does not fully satisfy the chromaticity/saturation aspect - despite keeping saturation constant while scaling lightness. Perceived result fits much better, than the above mentioned pure LCH Lightness blend mode.

The effect on Luminance blending is also described here: https://rawpedia.rawtherapee.com/Exposure#Luminance. The recommendation: "If you want to manually counter-act the desaturation, using the L*a*b* Chromaticity slider is a more neutral way of compensating for it..."

### Summary of motivation
Task: Find find the “correct” chroma after remapping the images luminance from source to target. Independent from how target was created. In my case I create target from source using tonecurves on luminance, sigmoid on luminance c2g, mantiuk or contrast equalizers from Darktable. Everything luminance-based - since it is rumored, luminance based workflows were more accurate.

## Examples
This image shows, the large original image. The contrast-enhanced one, that looks desaturated and the one processed by this gegl-HDR-ColorMapper.

<img src="https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/7f5c92ee-cfe1-443c-b268-6d441895a48f" width="600">

Now the same with the HDR RangeCompressor Mantiuk 06. HDR itself looks faded from color perspective.

<img src="https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/b9cf00a5-8798-4d45-8895-17105cb62cb2" width="600">

And now a more massive version: what I often do is, to create an grayscale image with c2g and use it as "exposure map" for the image (aka luminance layer mode in GIMP).

<img src="https://github.com/immanuelsch/gegl-HDR-ColorMapper/assets/23322212/a9b28d39-871f-4ca9-8917-83e94d848079" width="600">

Another example here:

<img src="https://github.com/user-attachments/assets/8de78478-6551-471c-9685-cd0bd30aaf85" width="600">

Or here in highRes:

<img src="https://github.com/user-attachments/assets/1ddd4cbd-8b2f-4f51-a538-adec6918cf80" width="600">



# The Concept
## Theory
- Increasing image contrast looks like compressing / squeezing the light parts of an image and the dark ones more together. So not only the lightness of each pixel is adopted, **but squeezing means, that the density of color pigments (saturation) increases also**.
- or the other way around: increasing contrast stretches things on a smaller lightness range to an wider range. So basically that is "stretching". Stretching means, that the same amount of color pigments is distributed in a bigger area. That reduces color density (saturation). This might be an explanation, why contrast stretched images "lack" pigments (saturation). To compensate for this effect, just increase the number of pigments (chromaticity) by the same ratio like the size of the surface increases.

The proposed algo takes this model into account and just blends in linear RGB light. Changing saturation is done by simply blending between the image and its desaturated equivalent. (Or, indeed it utilizes parts of the HSL(cie) color model, where L corresponds to CIE Y.)

The algo can cope with non-whitebalanced images and offers therefore an selector to define "physical white" - things that are perceived white under standard lighting conditions. It multiplies to neutral, does the changes in saturation, and afterward it puts the original tint back on the image.

**Also the method differs massively from just scaling overall image saturation. The math adopts each single pixel according to its appropriate new saturation. Areas with decreased contrast will be desaturated and chromaticity will be boosted in areas with increased contrast.**

Currently the operation is done as gegl-operation, but it could also be implemented als layer mode.

The "aux" input reads the "original" image with a consistent natural color. The layer where the gegl-op runs holds the desired luminance of the final image.

<img src="https://github.com/user-attachments/assets/0303385f-c4b4-4013-a1be-8c4b80fc17a4" width="400">



## Weaknesses
- contrast can sometimes only be "read out" (reverse engineered) from the image (by determining linear image gradient divided by luminance). Preference should always be to directly derive contrast changes. From changes in tone curve, for example by making the Chroma Channel dependent on Luminance based tone curve.
- the algo is not resilient by nature to image noise, that is indeed some kind of contrast (image gradient). Handling noise is currently done by some experimental smoothening filters.
- **top challenge currently**: target contrast of "0" leads to banding / halos especially for algorithms that compress the tone curve locally. Small contrast in source image in combination with zero contrast in regions of target image results in "0.0 div by something-nearly-zero"


# Some tech details
This drawing shows the principle of contrast-dependent chroma-scaling.
![ColorPhysics](https://github.com/user-attachments/assets/c3e76a7e-337d-450a-9b11-5dd081cc7e82)

So increasing contrast increases the density of color pigments, because it compresses all color pigments into the smaller space.

Some key-points to think about:
- reducing contrast to "0" will result in a luminance-constant image with neutral gray (no particles, totally desaturated (ideal for desaturating highlights or shadows in s-shaped contrast curves (like RGB filmic in Darktable does - but with complex math)).
- how can even (luminance-constant) coloured surfaces be represented?

Thus I played around with gegl:image-density as an alternative that tries to project the image to an flat, contrast-less image. This approach has to be investigated later. Inbetween I modified the gegl:image-gradient operation to:

1) operate on CIE Y channel only as it pursues an luminance-based tone mapping approach.
2) operate in linear light to stay in scene referred workflow as long as possible.
3) divide gradient by luminance (to make gradient independent from gegl:exposure. Global exposure changes image gradient but relative image gradient stays constant.)

## tonecurves
Just an example, why I prefer the luminance-based workflow over a toncurve in HSV color model. I applied this tonecurve (gamma 2.2):

<img src="https://github.com/user-attachments/assets/18fa2de5-6fe9-4730-9542-4eb713e15d08" width="400">

to an example color patch. Clearly to be seen: HSV tone curve up left does not really reduce luminance from colors. The luminance blend mode does really reduce brightness of the image but lacks saturation.

<img src="https://github.com/user-attachments/assets/7c9e3b7e-82f3-4611-88dd-6ad0ff767764" width="500">

So the best result seems to be lower row right picture.


**This page is WIP!**

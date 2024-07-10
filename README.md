# gegl-HDR-ColorMapper
Chromaticity mapping algorithm for range compression or contrast adjustment.

I'm the reporter of https://gitlab.gnome.org/GNOME/gegl/-/issues/171 that ended up in a native blending-based (native) saturation approach.
And I also published some code that lead Gimp to implement the Lightness Blend mode https://bugzilla.gnome.org/show_bug.cgi?id=753163.
To make things clear: I'm not a programmer - so my code looks ugly.

# Motivation
A lot of pictures look very desaturated when increasing image contrast. All receipes do not really help in a full extent: They adopt lightness/luminance and luminance contrast but to not map color correct form a color-intensity perspective.
- using GIMP LCH Lightness blend mode to adopt image contrast
- even using GIMP Lightness blend mode - that I brought up - does not fully satisfy the chromaticity/saturation aspect - even it satisfies saturation receptioni much better, than the above mentioned.

Examples:

WIP

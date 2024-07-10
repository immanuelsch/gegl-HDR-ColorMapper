/* This file is an image processing operation for GEGL
 *
 * GEGL is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * GEGL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GEGL; if not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright 2006 Øyvind Kolås <pippin@gimp.org>
 *           2022 Liam Quin <slave@fromoldbooks.org>
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

property_color (wp_color, _("Color"), "white")
    description (_("The color that represents the whitepoint"))
//    ui_meta     ("role", "color-primary")

//    property_double (viewing_angle, _("Horizontal Viewing angle in degree"), 360.0)
//    description (_("Viewing Angle of the image in degree horizontal. Image section of 360 degree"))
//    value_range (1.0, 360 ) /* arbitrarily large number */
//    ui_range(5.0, 180.0) /* for the slider in the user interface */

// property_double (image_width, _("Image Width"), 5196)
//    description (_("horizontal width of image in pixels / better chose min (width, height)"))
//    value_range (1.0, 10000) /* arbitrarily large number */
//    ui_range(300, 8000) /* for the slider in the user interface */

property_double (chroma_scale_gamma, _("scale chroma adoption"), 0.4545)
    description (_("Scales the Chroma effect of this plugin. Empiric best fit is 0.454 (c2g) - 1.0"))
    value_range (0.1, 3.0) /* arbitrarily large number */
    ui_range(0.1, 1.0) /* for the slider in the user interface */

property_double (denoise_radius_contrastfactor, _("denoise radius contrast factor"), 1.5)
    description (_("radius of median-denoising operation on contrast scaling factor"))
    value_range (0.1, 100.0) /* arbitrarily large number */
    ui_range(0.1, 10.0) /* for the slider in the user interface */
    ui_gamma (3.0)

property_int (denoise_radius, _("denoise radius luminance"), 3)
    description (_("radius of selective gaussian blur operation on grayscale images"))
    value_range (1, 100) /* arbitrarily large number */
    ui_range(1, 10) /* for the slider in the user interface */

property_double (max_dev, _("maximum deviation selective blurring"), 0.01f)
    description (_("max deviaton of selective gaussian blur operation on grayscale images"))
    value_range (0.0f, 100.0f) /* arbitrarily large number */
    ui_range(0.0f, 1.0f) /* for the slider in the user interface */
    ui_gamma (3.0)

#else

#include <gegl-plugin.h>

#define GEGL_OP_META
#define GEGL_OP_NAME     exposure_map
#define GEGL_OP_C_SOURCE exposure_map.c

#include "gegl-op.h"


static void attach(GeglOperation *operation)
{
  GeglNode *gegl = operation->node;
  GeglNode *yNew_in;                            // input
  GeglNode *output;                             // output
  GeglNode *old;                                // aux
  GeglNode *old_wb;                             // aux whitebalanced
  GeglNode *new_unc, *new_comp;                 // image new with chromaticity unchanged and chromacity compensated
  GeglNode *yNew, *yOld;                        // luminance
  GeglNode *yNewDenoise, *yOldDenoise;          // contrast (image gradient relative to luminance)
  GeglNode *cNew, *cOld;                        // contrast (image gradient relative to luminance)
  GeglNode *scale_exposure;                     // ratio of luminance and of contrast new vs old
  GeglNode *scale_contrast_without_gamma;       // ratio of luminance and of contrast new vs old
  GeglNode *scale_contrast;                     // adds gamma tuning of chromaticity adoption
  GeglNode *color_new, *color_new_comp, *final; // extracted color information of new image (rgb-y) and scaled (contrast-compensated) color information
  GeglNode *whitepoint, *y_whitepoint, *wp;     // color considered as white - untouched with regard to saturation adaption
  GeglNode *factor2neutral;                     // factor to convert to neutral
  GeglNode *scale_contrast_raw;                 // denoise of chromaticity adoption

//  GeglNode *cDebug;                            // contrast (image gradient relative to luminance)  
//  GeglNode *yDebug;                           // contrast (image gradient relative to luminance)  
  
//  GeglProperties  *o          = GEGL_PROPERTIES (operation);

//  GeglNode *gradient_rel_y0, *gradient_rel_y1, *exposure_blend, *y0, *gradient_ratio, *r_gradient_ratio, *color_plain, *color_boost, *exposure_ratio, *white, *rgbk, *yr, *final, *final2;

// map current layer (yNew_in), reference layer (original) and output (result of gegl op)
  yNew_in    = gegl_node_get_input_proxy (gegl, "input");
  old        = gegl_node_get_input_proxy (gegl, "aux");
  output     = gegl_node_get_output_proxy (gegl, "output");

// graph branch 0:
//  whitepoint = gegl_node_new_child (gegl, "operation", "gegl:color", "value", gegl_color_new ("rgb(1.0,1.0,1.0)"), NULL);
  wp = gegl_node_new_child (gegl, "operation", "gegl:color", NULL);
  whitepoint = gegl_node_new_child (gegl, "operation", "gegl:crop", NULL);
  gegl_node_connect_from (whitepoint, "aux", old, "output");
//  y_whitepoint = gegl_node_new_child (gegl, "operation", "gegl:gray", NULL);
  y_whitepoint = gegl_node_new_child (gegl, "operation", "gegl:saturation", "scale", 0.0, NULL);

  factor2neutral = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (factor2neutral, "aux", whitepoint, "output");
  gegl_node_link_many (wp, whitepoint, y_whitepoint, factor2neutral, NULL);

  old_wb = gegl_node_new_child (gegl, "operation", "gegl:multiply", NULL);
  gegl_node_connect_from (old_wb, "aux", factor2neutral, "output");
  gegl_node_link (old, old_wb);

// graph branch 1:
  // make sure new image / current layer is grayscale
  yNew = gegl_node_new_child (gegl, "operation", "gegl:saturation", "scale", 0.0, NULL);
//  cNew = gegl_node_new_child (gegl, "operation", "immanuel:image-gradient-rel", NULL);
  yNewDenoise = gegl_node_new_child (gegl, "operation", "gegl:gaussian-blur-selective", NULL);
  cNew = gegl_node_new_child (gegl, "operation", "immanuel:image-density", NULL);
  gegl_node_link_many (yNew_in, yNew, yNewDenoise, cNew, NULL);

// graph branch 2:
  // make sure reference image / "aux" is grayscale

  //  yOld = gegl_node_new_child (gegl, "operation", "gegl:gray", NULL);
  yOld = gegl_node_new_child (gegl, "operation", "gegl:saturation", "scale", 0.0, NULL);
//  yOldY = gegl_node_new_child (gegl, "operation", "gegl:convert-format", "format", "Y float", NULL);
//  cOld = gegl_node_new_child (gegl, "operation", "immanuel:image-gradient-rel", NULL);
  yOldDenoise = gegl_node_new_child (gegl, "operation", "gegl:gaussian-blur-selective", NULL);
  cOld = gegl_node_new_child (gegl, "operation", "immanuel:image-density", NULL);  //image dimension ranzig
 gegl_node_link_many (old, yOld, yOldDenoise, cOld, NULL);

/*
  gegl_node_link_many (old_wb, yOld, NULL);
  gegl_node_link_many (whitepoint, cOld, NULL);
*/

// link graph branches to determine both scaling factors
  // divide new luminance by old luminance
  scale_exposure = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (scale_exposure, "aux", yOld, "output");
  gegl_node_link (yNew, scale_exposure);


  scale_contrast_without_gamma = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  scale_contrast = gegl_node_new_child (gegl, "operation", "gegl:gamma", "value", 0.5, NULL);
//  scale_contrast_raw = gegl_node_new_child (gegl, "operation", "gegl:gaussian-blur", NULL);
  scale_contrast_raw = gegl_node_new_child (gegl, "operation", "gegl:median-blur", "high-precision", 1, NULL);
  gegl_node_connect_from (scale_contrast_without_gamma, "aux", cOld, "output");
//  gegl_node_link (cNew, scale_contrast);
  gegl_node_link_many (cNew, scale_contrast_without_gamma, scale_contrast_raw, scale_contrast, NULL);

// graph branch 3:
  // scale exposure (all rgb values) of original/old/aux with exposure scaling factor exactly like gimp:layermode "layer-mode" "luminance"
  new_unc = gegl_node_new_child (gegl, "operation", "gegl:multiply", NULL);
  gegl_node_connect_from (new_unc, "aux", scale_exposure, "output");

  // extract color from uncompensated new image (rgb-y)
  color_new = gegl_node_new_child (gegl, "operation", "gegl:subtract", NULL);
  gegl_node_connect_from (color_new, "aux", yNew, "output");

  // scale extracted color with scale_contrast
  color_new_comp = gegl_node_new_child (gegl, "operation", "gegl:multiply", NULL);
  gegl_node_connect_from (color_new_comp, "aux", scale_contrast, "output");

  // add scaled color back to grayscale image
  new_comp = gegl_node_new_child (gegl, "operation", "gegl:add", NULL);
  gegl_node_connect_from (new_comp, "aux", yNew, "output");

  // invert WhiteBalance
  final = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (final, "aux", factor2neutral, "output");
  
  gegl_node_link_many (old_wb, new_unc, color_new, color_new_comp, new_comp, final, output, NULL);

      // debug_output
/*
  cDebug = gegl_node_new_child (gegl, "operation", "immanuel:image-density", NULL);
  yDebug = gegl_node_new_child (gegl, "operation", "gegl:saturation", "scale", 0.0, "colorspace", "Native", NULL);
  gegl_node_link_many (old_wb, cDebug, output, NULL);
//this works with old but not with old_wb
*/

  /* arrange to pass on our two properties to their respective nodes */

// gegl_operation_meta_redirect (operation, "viewing_angle", cNew, "viewing_angle");
// gegl_operation_meta_redirect (operation, "viewing_angle", cOld, "viewing_angle");

// gegl_operation_meta_redirect (operation, "image_width", cNew, "image_width");
// gegl_operation_meta_redirect (operation, "image_width", cOld, "image_width");
gegl_operation_meta_redirect (operation, "wp_color", wp, "value");
gegl_operation_meta_redirect (operation, "chroma_scale_gamma", scale_contrast, "value");

gegl_operation_meta_redirect (operation, "denoise_radius", yNewDenoise, "blur-radius");
gegl_operation_meta_redirect (operation, "denoise_radius", yNewDenoise, "max-delta");
gegl_operation_meta_redirect (operation, "denoise_radius", yOldDenoise, "blur-radius");
gegl_operation_meta_redirect (operation, "denoise_radius", yOldDenoise, "max-delta");
gegl_operation_meta_redirect (operation, "denoise_radius_contrastfactor", scale_contrast_raw, "radius");

}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationMetaClass *operation_meta_class = GEGL_OPERATION_META_CLASS (klass);
  GeglOperationClass       *operation_class = GEGL_OPERATION_CLASS (klass);

  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
    "title",          _("map exposure with chromaticity scaling"),
    "name",           "immanuel:exposure_map",
    "categories",     "Artistic",
    "description",  _("maps exposure according to the luminance of layer. Includes contrast-aware chromaticity scaling"),
    NULL);

}


#endif

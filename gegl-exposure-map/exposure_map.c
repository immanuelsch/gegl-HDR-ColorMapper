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
 *           2024 Immanuel Schaffer
 */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

/* define gegl properties
 * for scaling the chroma scaling effect of this plugin
 */
property_double (chroma_scale_gamma, _("strength"), 1.0)
    description (_("Scales chroma-adoption effect of plugin. Usually ranges between 0.454 (c2g) - 1.0"))
    value_range (0.1, 3.0) /* arbitrarily large number */
    ui_range(0.1, 1.0) /* for the slider in the user interface */

/* define gegl properties
 * for whitepoint in case the algo runs on non-whitebalanced images.
 * Select the color that normally represents white / neutral gray
 */
property_color (wp_color, _("neutral / white representation"), "white")
    description (_("Chose a color that represents white or neutral gray."))
//    ui_meta     ("role", "color-primary")
    

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
  GeglNode *cNew, *cOld;                        // contrast (image gradient relative to luminance)
  GeglNode *scale_exposure;                     // ratio of luminance and of contrast new vs old
  GeglNode *scale_contrast_without_gamma;       // ratio of luminance and of contrast new vs old
  GeglNode *scale_contrast;                     // adds gamma tuning of chromaticity adoption
  GeglNode *color_new, *color_new_comp, *final; // extracted color information of new image (rgb-y) and scaled (contrast-compensated) color information
  GeglNode *whitepoint, *y_whitepoint, *wp;     // color considered as white - untouched with regard to saturation adaption
  GeglNode *factor2neutral;                     // factor to convert to neutral
  GeglNode *clip_neg, *clip_pos;
  GeglNode *gray, *subtract_gray, *desaturate_color, *add_desaturated;
  GeglNode *overcolor_neg, *overcolor_pos;
  GeglNode *div_by_overcolor_neg;
  GeglNode *invert_linear, *reinvert_linear;
  GeglNode *HSV_neg;
  GeglNode *debug;
  
  

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
  cNew = gegl_node_new_child (gegl, "operation", "immanuel:image-gradient-rel", NULL);
//  cNew = gegl_node_new_child (gegl, "operation", "immanuel:image-density", NULL);
  gegl_node_link_many (yNew_in, yNew, cNew, NULL);

// graph branch 2:
  // make sure reference image / "aux" is grayscale

  //  yOld = gegl_node_new_child (gegl, "operation", "gegl:gray", NULL);
  yOld = gegl_node_new_child (gegl, "operation", "gegl:saturation", "scale", 0.0, NULL);
//  yOldY = gegl_node_new_child (gegl, "operation", "gegl:convert-format", "format", "Y float", NULL);
  cOld = gegl_node_new_child (gegl, "operation", "immanuel:image-gradient-rel", NULL); 
// cOld = gegl_node_new_child (gegl, "operation", "immanuel:image-density", NULL);  //image dimension ranzig
  gegl_node_link_many (old, yOld, cOld, NULL);


// link graph branches to determine both scaling factors
  // divide new luminance by old luminance
  scale_exposure = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (scale_exposure, "aux", yOld, "output");
  gegl_node_link (yNew, scale_exposure);


  scale_contrast_without_gamma = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  scale_contrast = gegl_node_new_child (gegl, "operation", "gegl:gamma", "value", 0.5, NULL);
  gegl_node_connect_from (scale_contrast_without_gamma, "aux", cOld, "output");
  gegl_node_link_many (cNew, scale_contrast_without_gamma, scale_contrast, NULL);

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
  
  gegl_node_link_many (old_wb, new_unc, color_new, color_new_comp, new_comp, final, NULL);

// graph branch 4:
  clip_neg = gegl_node_new_child (gegl, "operation", "gegl:rgb-clip", "clip-high", TRUE, "clip-low", FALSE, "high-limit", 0.0, NULL);
  gray = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (gray, "aux", factor2neutral, "output");
  overcolor_neg = gegl_node_new_child (gegl, "operation", "gegl:subtract", NULL);
  gegl_node_connect_from (overcolor_neg, "aux", clip_neg, "output");
  div_by_overcolor_neg = gegl_node_new_child (gegl, "operation", "gegl:divide", NULL);
  gegl_node_connect_from (div_by_overcolor_neg, "aux", overcolor_neg, "output");
  invert_linear = gegl_node_new_child (gegl, "operation", "gegl:invert-linear", NULL);
  HSV_neg = gegl_node_new_child (gegl, "operation", "gegl:component-extract", "component", 5, NULL);
  reinvert_linear = gegl_node_new_child (gegl, "operation", "gegl:invert-linear", NULL);

  gegl_node_link (final, clip_neg);
  gegl_node_link_many (yNew, gray, overcolor_neg, NULL);
  gegl_node_link_many (gray, div_by_overcolor_neg, invert_linear, HSV_neg, reinvert_linear, NULL);

// graph branch 5: reduce saturation
  subtract_gray = gegl_node_new_child (gegl, "operation", "gegl:subtract", NULL);
  gegl_node_connect_from (subtract_gray, "aux", gray, "output");
  desaturate_color = gegl_node_new_child (gegl, "operation", "gegl:multiply", NULL);
  gegl_node_connect_from (desaturate_color, "aux", reinvert_linear, "output");
  add_desaturated = gegl_node_new_child (gegl, "operation", "gegl:add", NULL);
  gegl_node_connect_from (add_desaturated, "aux", gray, "output");

//  debug = gegl_node_new_child (gegl, "operation", "gegl:component-extract", "component", 5, NULL);
//  gegl_node_link (final, debug);

  gegl_node_link_many (final, subtract_gray, desaturate_color, add_desaturated, NULL);
  gegl_node_link (add_desaturated, output);
   
// meta redirects
  
gegl_operation_meta_redirect (operation, "wp_color", wp, "value");
gegl_operation_meta_redirect (operation, "chroma_scale_gamma", scale_contrast, "value");

}


static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationMetaClass *operation_meta_class = GEGL_OPERATION_META_CLASS (klass);
  GeglOperationClass       *operation_class = GEGL_OPERATION_CLASS (klass);

  operation_class->attach = attach;

  gegl_operation_class_set_keys (operation_class,
    "title",          _("HDR ColorMapper"),
    "name",           "immanuel:exposure_map",
    "categories",     "Artistic",
    "description",  _("map colors of colorimetric consistent source image (aux) to the luminance channel of target - scaling saturation proportional with contrast change"),
    NULL);

}


#endif

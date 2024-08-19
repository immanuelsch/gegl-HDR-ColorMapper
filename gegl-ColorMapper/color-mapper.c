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
 * Copyright 2016 Thomas Manni <thomas.manni@free.fr>
 * Authors:  2024 Immanuel Schaffer
 */

 /* compensate for chromaticity effects in luminance remapping operations */

#include "config.h"
#include <glib/gi18n-lib.h>

#ifdef GEGL_PROPERTIES

enum_start (gegl_colormapper_technology)
   enum_value (GEGL_COLORMAPPER_DEFAULT, "default", N_("Default"))
   enum_value (GEGL_COLORMAPPER_DEFAULT_RGB_UNLIMITED, "default rgb unlimited", N_("Default RGB unlimited"))
   enum_value (GEGL_COLORMAPPER_GRADIENT_RATIO, "gradient_ratio", N_("Gradient Ratio"))
   enum_value (GEGL_COLORMAPPER_CHROMATICITY, "chromaticity", N_("HSY Chromaticity"))
   enum_value (GEGL_COLORMAPPER_SATURATION, "saturation", N_("HSY Saturation"))
   enum_value (GEGL_COLORMAPPER_YGRAD_AUX, "linear aux gradient", N_("Linerar Gradient of aux"))
enum_end (GeglColorMapperTechology)

property_color (WhiteRepresentation, _("neutral / white representation"), "white")
    description (_("Chose a color that represents white or neutral gray."))
    
property_enum (technology, _("output mode"),
               GeglColorMapperTechology, gegl_colormapper_technology,
               GEGL_COLORMAPPER_DEFAULT)
  description (_("Technology Chromaticity Compensation"))

property_double (scale, _("scale strengh of effect"), 1.0)
  description(_("Strength of chromaticity adoption effect."))
  value_range   (0.0, 2.0)
  ui_range      (0.0, 2.0)
//  ui_digits     (5)
//  ui_gamma      (2.0)

property_double (globalSaturation, _("global Saturation"), 1.0)
  description(_("overall saturation whitepoint compensated"))
  value_range   (0.0, 5.0)
  ui_range      (0.0, 2.0)
//  ui_digits     (7)
//  ui_gamma      (2.0)

property_boolean (perceptual, _("perceptual"), TRUE)
  description (_("chroma compensation based on perceptual lightness"))


#else

#define GEGL_OP_COMPOSER
#define GEGL_OP_NAME         color_mapper
#define GEGL_OP_C_SOURCE     color-mapper.c

#define POW2(x) ((x)*(x))

#include "gegl-op.h"

static void
prepare (GeglOperation *operation)
{
  const Babl *format = babl_format_with_space ("RGBA float",
                           gegl_operation_get_source_space (operation, "input"));

  gegl_operation_set_format (operation, "input",  format);
  gegl_operation_set_format (operation, "aux",    format);
  gegl_operation_set_format (operation, "output", format);
}

static GeglRectangle
get_bounding_box (GeglOperation *operation)
{
  GeglRectangle  result = { 0, 0, 0, 0 };
  GeglRectangle *in_rect;

  in_rect = gegl_operation_source_get_bounding_box (operation, "input");
  if (in_rect)
    {
      result = *in_rect;
    }

  return result;
}

static GeglRectangle
get_enlarged_input (GeglOperation       *operation,
                    const GeglRectangle *input_region)
{
//  GeglProperties *o = GEGL_PROPERTIES (operation);
  GeglRectangle   rect;

  /* relevant for output computation is pixel including the direct neigbouring pixels */
  rect.x       = input_region->x - 1;
  rect.y       = input_region->y - 1;
  rect.width   = input_region->width  + 2;
  rect.height  = input_region->height + 2;

  return rect;
}

static GeglRectangle
get_required_for_output (GeglOperation       *operation,
                         const gchar         *input_pad,
                         const GeglRectangle *region)
{
  GeglRectangle   rect;
  GeglRectangle   defined;

  defined = gegl_operation_get_bounding_box (operation);
  gegl_rectangle_intersect (&rect, region, &defined);

  if (rect.width  != 0 && rect.height != 0)
    {
      rect = get_enlarged_input (operation, &rect);
    }

  return rect;
}

static GeglRectangle
get_invalidated_by_change (GeglOperation       *operation,
                           const gchar         *input_pad,
                           const GeglRectangle *input_region)
{
  return get_enlarged_input (operation, input_region);
}


static gboolean
color_mapper (GeglBuffer                *input,
              const GeglRectangle       *src_rect,
              GeglBuffer                *aux,
              GeglBuffer                *output,
              const GeglRectangle       *dst_rect,
              GeglColorMapperTechology  technology,
              gboolean                  perceptual,
              gdouble                   scale,
              GeglColor                 *WhiteRepresentation,
              gdouble                   globalSaturation,
              gint                      level)

{
  const Babl *space = gegl_buffer_get_format (output);
  const Babl *gray_format = babl_format_with_space ("Y float", space);
  const Babl *format = babl_format_with_space ("RGBA float", space);

  
  /* input grayscale */
  gfloat *row1_in, *row2_in, *row3_in;
  gfloat *top_ptr_Yin, *mid_ptr_Yin, *down_ptr_Yin, *tmp_ptr_Yin;

  /* aux grayscale */
  gfloat *row1_aux, *row2_aux, *row3_aux;
  gfloat *top_ptr_Yaux, *mid_ptr_Yaux, *down_ptr_Yaux, *tmp_ptr_Yaux;

  /* in, aux full buffer */
  gfloat *row_in_buf, *row_aux_buf;
  
  gfloat *row_out;
  gfloat NeutralRepresentation[4], NeutralRepresentationDesaturated[1], tinted2neutral[3], neutral2tinted[3];
  gint    x, y;

  GeglRectangle row_rect;
  GeglRectangle out_rect;

  gegl_color_get_pixel (WhiteRepresentation, format, &NeutralRepresentation);
  gegl_color_get_pixel (WhiteRepresentation, gray_format, &NeutralRepresentationDesaturated);

  row_in_buf  = g_new (gfloat, dst_rect->width * 4);
  row_aux_buf = g_new0 (gfloat, dst_rect->width * 4);

  row1_in = g_new (gfloat, src_rect->width);
  row2_in = g_new (gfloat, src_rect->width);
  row3_in = g_new (gfloat, src_rect->width);
  row1_aux = g_new0 (gfloat, src_rect->width);
  row2_aux = g_new0 (gfloat, src_rect->width);
  row3_aux = g_new0 (gfloat, src_rect->width);

  row_out = g_new0 (gfloat, dst_rect->width * 4);  
  
  top_ptr_Yin  = row1_in;
  mid_ptr_Yin  = row2_in;
  down_ptr_Yin = row3_in;
  top_ptr_Yaux  = row1_aux;
  mid_ptr_Yaux  = row2_aux;
  down_ptr_Yaux = row3_aux;

  row_rect.width = src_rect->width;
  row_rect.height = 1;
  row_rect.x = src_rect->x;
  row_rect.y = src_rect->y;

  out_rect.x      = dst_rect->x;
  out_rect.width  = dst_rect->width;
  out_rect.height = 1;

  /* fill gegl buffer row-wise with grayscale data */  
  gegl_buffer_get (input, &row_rect, 1.0, gray_format, top_ptr_Yin,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  if (aux)
    {
      gegl_buffer_get (aux, &row_rect, 1.0, gray_format, top_ptr_Yaux,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
    }
    
  row_rect.y++;
  gegl_buffer_get (input, &row_rect, 1.0, gray_format, mid_ptr_Yin,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  if (aux)
    {
      gegl_buffer_get (aux, &row_rect, 1.0, gray_format, mid_ptr_Yaux,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
    }

  /* factor to tranform a tinted image to an neutral one */    
  tinted2neutral[0] = NeutralRepresentationDesaturated[0] / NeutralRepresentation[0];  
  tinted2neutral[1] = NeutralRepresentationDesaturated[0] / NeutralRepresentation[1];  
  tinted2neutral[2] = NeutralRepresentationDesaturated[0] / NeutralRepresentation[2];  
  neutral2tinted[0] = 1.0 / tinted2neutral[0];  
  neutral2tinted[1] = 1.0 / tinted2neutral[1];  
  neutral2tinted[2] = 1.0 / tinted2neutral[2];  
    
  /* loop rows and compute contrast ratio between both input and aux */    
  for (y = dst_rect->y; y < dst_rect->y + dst_rect->height; y++)
    {
      row_rect.y = y + 1;
      out_rect.y = y;

      gegl_buffer_get (input, &row_rect, 1.0, gray_format, down_ptr_Yin,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
      gegl_buffer_get (input, &out_rect, 1.0, format, row_in_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
      if (aux)
        {
          gegl_buffer_get (aux, &row_rect, 1.0, gray_format, down_ptr_Yaux,
                           GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
          gegl_buffer_get (aux, &out_rect, 1.0, format, row_aux_buf,
                           GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
        }

      for (x = 1; x < row_rect.width - 1; x++)
        {
          gfloat dx_in, dx_aux;
          gfloat dy_in, dy_aux;
          gfloat luminanceblended[3], luminanceblended_colorscaled[3], tinted_gray[3], tinted_gray_aux[3];
          gfloat chroma_aux[3];
          gfloat GradientRatio;
          gfloat ChromaAdoptionFactor;
          gint idx = 0;
          
          /* contrast of input div by aux */
          gfloat luminance_ratio;
          gfloat saturation_clip_negative[3], saturation_clip_positive[3];
          gfloat saturation_clip_negative_min, saturation_clip_positive_min;
          gfloat Chroma_HSY_aux;
          gfloat GradientYin, GradientYaux;
          
          /* compute dx, dy and YSum for input and aux buffer */          
          dx_in  = (mid_ptr_Yin[(x-1)]  - mid_ptr_Yin[(x+1)]);
          dx_aux = (mid_ptr_Yaux[(x-1)] - mid_ptr_Yaux[(x+1)]);
          dy_in  = (top_ptr_Yin[x]  - down_ptr_Yin[x]);
          dy_aux = (top_ptr_Yaux[x] - down_ptr_Yaux[x]);
          GradientYin  = 0.5 * sqrtf (POW2(dx_in)  + POW2(dy_in));
          GradientYaux = 0.5 * sqrtf (POW2(dx_aux) + POW2(dy_aux));

          /* computing luminance ratio */                    
          luminance_ratio = (fabs(mid_ptr_Yaux[x]) > FLT_MIN) ? (mid_ptr_Yin[x] / mid_ptr_Yaux[x]) : 0.0; 
          
          /* computing gradient ratio */
          GradientRatio = (GradientYaux > FLT_MIN) ? (GradientYin / GradientYaux) : 1.0;

          if (perceptual == TRUE)
            ChromaAdoptionFactor = (mid_ptr_Yin[x] > FLT_MIN) ? powf (mid_ptr_Yaux[x] * GradientRatio / mid_ptr_Yin[x], 1.0 / 2.2) : 1.0;
          else
            ChromaAdoptionFactor = (mid_ptr_Yin[x] > FLT_MIN) ? mid_ptr_Yaux[x] * GradientRatio / mid_ptr_Yin[x] : 1.0;

          // ChromaAdoptionFactor = (ChromaAdoptionFactor - 1.0) * scale * 0.5 + 1.0;

          /* change ChromaAdoption to be 1.0 on GradientYin or GradientYaux == 0 or GradientYin == GradientYaux */
          // ChromaAdoptionFactor = 1.0f - M_PI_2 * powf (GradientRatio, scale * 0.5f) * cosf (M_PI_2 * powf (GradientRatio, scale * 0.5f));

          /* FIXME: limit max ChromaAdption factor <2pi before cos */
          
          // ChromaAdoptionFactor = fmin (ChromaAdoptionFactor, powf (4.0f, (1.0f / (0.4f * scale))) );
          // ChromaAdoptionFactor = 1.0f - M_PI_2 * powf (ChromaAdoptionFactor, scale * 0.4f) * cosf (M_PI_2 * powf (ChromaAdoptionFactor, scale * 0.4f));
          ChromaAdoptionFactor = 3.0 * M_PI_2 * (1.0 - 1.0 / (0.5 * powf (ChromaAdoptionFactor, scale * 0.6) + 1.0));
          ChromaAdoptionFactor = 1.0 - ChromaAdoptionFactor * cosf (ChromaAdoptionFactor);
  
  
          /* compute R (+0), G (+1), B (+2) */
          idx = (x-1) * 4;

          /* scale exposure (analogue to luminance layer mode) */
          luminanceblended[0] = luminance_ratio * row_aux_buf[idx + 0];
          luminanceblended[1] = luminance_ratio * row_aux_buf[idx + 1];
          luminanceblended[2] = luminance_ratio * row_aux_buf[idx + 2];

          /* grayscale image under current lighting condiditions */
          tinted_gray[0] = mid_ptr_Yin[x] * neutral2tinted[0];
          tinted_gray[1] = mid_ptr_Yin[x] * neutral2tinted[1];
          tinted_gray[2] = mid_ptr_Yin[x] * neutral2tinted[2];

          /* grayscale image aux under current lighting condiditions */
          tinted_gray_aux[0] = mid_ptr_Yaux[x] * neutral2tinted[0];
          tinted_gray_aux[1] = mid_ptr_Yaux[x] * neutral2tinted[1];
          tinted_gray_aux[2] = mid_ptr_Yaux[x] * neutral2tinted[2];

          chroma_aux[0] = row_aux_buf[idx + 0] - tinted_gray_aux[0];
          chroma_aux[1] = row_aux_buf[idx + 1] - tinted_gray_aux[1];
          chroma_aux[2] = row_aux_buf[idx + 2] - tinted_gray_aux[2];

          /* chromaticity equivalent in HSY Color Model of in-buffer - before chroma scaling */
          Chroma_HSY_aux = sqrtf (POW2(chroma_aux[0]) + POW2(chroma_aux[1]) + POW2(chroma_aux[2]) - (chroma_aux[0] * chroma_aux[1] + chroma_aux[0] * chroma_aux[2] + chroma_aux[1] * chroma_aux[2]));
                    
          /* new algorithm */
          // luminanceblended_colorscaled[0] = (tinted_gray[0] + GradientRatio * chroma_aux[0]) * scale + luminanceblended[0] * (1.0 - scale);
          // luminanceblended_colorscaled[1] = (tinted_gray[1] + GradientRatio * chroma_aux[1]) * scale + luminanceblended[1] * (1.0 - scale);
          // luminanceblended_colorscaled[2] = (tinted_gray[2] + GradientRatio * chroma_aux[2]) * scale + luminanceblended[2] * (1.0 - scale);

          /* new algorithm perceptual*/
          // luminanceblended_colorscaled[0] = (tinted_gray[0] + luminance_ratio * chroma_aux[0] * ChromaAdoptionFactor) * scale + luminanceblended[0] * (1.0 - scale);
          // luminanceblended_colorscaled[1] = (tinted_gray[1] + luminance_ratio * chroma_aux[1] * ChromaAdoptionFactor) * scale + luminanceblended[1] * (1.0 - scale);
          // luminanceblended_colorscaled[2] = (tinted_gray[2] + luminance_ratio * chroma_aux[2] * ChromaAdoptionFactor) * scale + luminanceblended[2] * (1.0 - scale);

          /* new algorithm perceptual - new scaling*/
          luminanceblended_colorscaled[0] = tinted_gray[0] + luminance_ratio * chroma_aux[0] * ChromaAdoptionFactor * globalSaturation;
          luminanceblended_colorscaled[1] = tinted_gray[1] + luminance_ratio * chroma_aux[1] * ChromaAdoptionFactor * globalSaturation;
          luminanceblended_colorscaled[2] = tinted_gray[2] + luminance_ratio * chroma_aux[2] * ChromaAdoptionFactor * globalSaturation;
          
          /* reduce saturation to better fit in rgb-range [0...1] */
          saturation_clip_negative[0] = tinted_gray[0] / (tinted_gray[0] - fmin ( luminanceblended_colorscaled[0], -0.00001f));
          saturation_clip_negative[1] = tinted_gray[1] / (tinted_gray[1] - fmin ( luminanceblended_colorscaled[1], -0.00001f));
          saturation_clip_negative[2] = tinted_gray[2] / (tinted_gray[2] - fmin ( luminanceblended_colorscaled[2], -0.00001f));
          saturation_clip_negative_min = fmin (saturation_clip_negative[0], fmin (saturation_clip_negative[1], saturation_clip_negative[2]));

          saturation_clip_positive[0] = fmax (luminanceblended_colorscaled[0] - fmax (1.0f, tinted_gray[0]), 0.0f) / (luminanceblended_colorscaled[0] - tinted_gray[0] + 0.00001f);
          saturation_clip_positive[1] = fmax (luminanceblended_colorscaled[1] - fmax (1.0f, tinted_gray[1]), 0.0f) / (luminanceblended_colorscaled[1] - tinted_gray[1] + 0.00001f);
          saturation_clip_positive[2] = fmax (luminanceblended_colorscaled[2] - fmax (1.0f, tinted_gray[2]), 0.0f) / (luminanceblended_colorscaled[2] - tinted_gray[2] + 0.00001f);
          saturation_clip_positive_min = 1.0f - fmax (saturation_clip_positive[0], fmax (saturation_clip_positive[1], saturation_clip_positive[2]));
          
          if (technology == GEGL_COLORMAPPER_GRADIENT_RATIO)
          {
            row_out[idx + 0] = row_out[idx + 1] = row_out[idx + 2] = GradientRatio;
          }
          else if (technology == GEGL_COLORMAPPER_CHROMATICITY)
          {
            row_out[idx + 0] = row_out[idx + 1] = row_out[idx + 2] = Chroma_HSY_aux;
          }
          else if (technology == GEGL_COLORMAPPER_SATURATION)
          {
            row_out[idx + 0] = row_out[idx + 1] = row_out[idx + 2] = Chroma_HSY_aux / mid_ptr_Yaux[x];
          }
          else if (technology == GEGL_COLORMAPPER_YGRAD_AUX)
          {
            row_out[idx + 0] = row_out[idx + 1] = row_out[idx + 2] = GradientYaux;
          }
          else if (technology == GEGL_COLORMAPPER_DEFAULT_RGB_UNLIMITED)
          {
            row_out[idx + 0] = luminanceblended_colorscaled[0];
            row_out[idx + 1] = luminanceblended_colorscaled[1];
            row_out[idx + 2] = luminanceblended_colorscaled[2];
          }
          else
          {
            row_out[idx + 0] = (luminanceblended_colorscaled[0] - tinted_gray[0]) * fmin (saturation_clip_positive_min, saturation_clip_negative_min) + tinted_gray[0];
            row_out[idx + 1] = (luminanceblended_colorscaled[1] - tinted_gray[1]) * fmin (saturation_clip_positive_min, saturation_clip_negative_min) + tinted_gray[1];
            row_out[idx + 2] = (luminanceblended_colorscaled[2] - tinted_gray[2]) * fmin (saturation_clip_positive_min, saturation_clip_negative_min) + tinted_gray[2];
          }

          /* keep alpha from in */
          row_out[idx + 3] = row_in_buf[idx + 3];
        }

      gegl_buffer_set (output, &out_rect, level, format, row_out,
                       GEGL_AUTO_ROWSTRIDE);

      tmp_ptr_Yin = top_ptr_Yin;
      top_ptr_Yin = mid_ptr_Yin;
      mid_ptr_Yin = down_ptr_Yin;
      down_ptr_Yin = tmp_ptr_Yin;

      tmp_ptr_Yaux = top_ptr_Yaux;
      top_ptr_Yaux = mid_ptr_Yaux;
      mid_ptr_Yaux = down_ptr_Yaux;
      down_ptr_Yaux = tmp_ptr_Yaux;
    }

  g_free (row1_in);
  g_free (row2_in);
  g_free (row3_in);
  g_free (row1_aux);
  g_free (row2_aux);
  g_free (row3_aux);
  g_free (row_out);
  g_free (row_in_buf);
  g_free (row_aux_buf);

  return TRUE;
}

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *aux,
         GeglBuffer          *output,
         const GeglRectangle *result,
         gint                 level)
{
  GeglProperties *o = GEGL_PROPERTIES (operation);
  gboolean        success;
  GeglRectangle   compute;

  compute = get_required_for_output (operation, "input", result);

  success = color_mapper (input, &compute,
                          aux,
                          output, result,
                          o->technology, o->perceptual,
                          o->scale, o->WhiteRepresentation, o->globalSaturation,
                          level);
  return success;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass              *operation_class;
  GeglOperationComposerClass      *composer_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  composer_class  = GEGL_OPERATION_COMPOSER_CLASS (klass);
  
  operation_class->prepare                   = prepare;
  operation_class->get_required_for_output   = get_required_for_output;
  operation_class->get_invalidated_by_change = get_invalidated_by_change;
  operation_class->get_bounding_box          = get_bounding_box;
  operation_class->opencl_support            = FALSE;

  composer_class->process           = process;
  composer_class->aux_label         = _("original colored image");
  composer_class->aux_description   = _("holds the original image with "
                                        "source colors.");


  gegl_operation_class_set_keys (operation_class,
    "name",        "immanuel:color-mapper",
    "title",       _("Color Mapper"),
    "categories",  "photo",
//    "reference-hash", "6cd95bf706d744b31b475b3500941f3c",
//    "reference-hashB", "3bc1f4413a06969bf86606d621969651",
    "description", _("Do color remapping appropriate suitable to luminance remapping operations"
                     "(includes chroma adoption do avoid (de)saturation effect)"),
    NULL);
}

#endif

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
   enum_value (GEGL_COLORMAPPER_APPROACH_1, "Approach 1", N_("Approach_1"))
   enum_value (GEGL_COLORMAPPER_APPROACH_2, "Approach 2", N_("Approach_2"))
   enum_value (GEGL_COLORMAPPER_APPROACH_3, "Approach 3", N_("Approach_3"))
enum_end (GeglColorMapperTechology)

property_color (WhiteRepresentation, _("neutral / white representation"), "white")
    description (_("Chose a color that represents white or neutral gray."))
    
property_enum (technology_chromaticity_compensation, _("Technology Chromaticity Compensation"),
               GeglColorMapperTechology, gegl_colormapper_technology,
               GEGL_COLORMAPPER_APPROACH_1)
  description (_("Technology Chromaticity Compensation"))

property_double (scale, _("scale strengh of effect"), 1.0)
  description(_("Strength of chromaticity adoption effect."))
  value_range   (0.0, 3.0)
  ui_range      (0.0, 2.0)
  
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
color_mapper (GeglBuffer          *input,
              const GeglRectangle *src_rect,
              GeglBuffer          *aux,
              GeglBuffer          *output,
              const GeglRectangle *dst_rect,
              gdouble              scale,
              GeglColor           *WhiteRepresentation,
              gint                 level)

{
  const Babl *space = gegl_buffer_get_format (output);
  const Babl *gray_format = babl_format_with_space ("Y float", space);
  const Babl *format = babl_format_with_space ("RGBA float", space);

  /* input grayscale */
  gfloat *row1_in, *row2_in, *row3_in;
  gfloat *top_ptr_in, *mid_ptr_in, *down_ptr_in, *tmp_ptr_in;

  /* aux grayscale */
  gfloat *row1_aux, *row2_aux, *row3_aux;
  gfloat *top_ptr_aux, *mid_ptr_aux, *down_ptr_aux, *tmp_ptr_aux;

  /* in, aux full buffer */
  gfloat *in_buf, *aux_buf;
  
  gfloat *row_out;
  gint    x, y;

  GeglRectangle row_rect;
  GeglRectangle out_rect;

  in_buf  = g_new (gfloat, dst_rect->width * dst_rect->height * 4);
  aux_buf = g_new0 (gfloat, dst_rect->width * dst_rect->height * 4);

  row1_in = g_new (gfloat, src_rect->width);
  row2_in = g_new (gfloat, src_rect->width);
  row3_in = g_new (gfloat, src_rect->width);
  row1_aux = g_new0 (gfloat, src_rect->width);
  row2_aux = g_new0 (gfloat, src_rect->width);
  row3_aux = g_new0 (gfloat, src_rect->width);

  row_out = g_new0 (gfloat, dst_rect->width * 4);  
  
  top_ptr_in  = row1_in;
  mid_ptr_in  = row2_in;
  down_ptr_in = row3_in;
  top_ptr_aux  = row1_aux;
  mid_ptr_aux  = row2_aux;
  down_ptr_aux = row3_aux;

  row_rect.width = src_rect->width;
  row_rect.height = 1;
  row_rect.x = src_rect->x;
  row_rect.y = src_rect->y;

  out_rect.x      = dst_rect->x;
  out_rect.width  = dst_rect->width;
  out_rect.height = 1;

  /* fill gegl buffer with colored data of in, aux */
  gegl_buffer_get (input, dst_rect, 1.0, format, in_buf,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  if (aux)
    {
      gegl_buffer_get (aux, dst_rect, 1.0, format, aux_buf,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
    }
 
  /* fill gegl buffer row-wise with grayscale data */  
  gegl_buffer_get (input, &row_rect, 1.0, gray_format, top_ptr_in,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  if (aux)
    {
      gegl_buffer_get (aux, &row_rect, 1.0, gray_format, top_ptr_aux,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
    }
    
  row_rect.y++;
  gegl_buffer_get (input, &row_rect, 1.0, gray_format, mid_ptr_in,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
  if (aux)
    {
      gegl_buffer_get (aux, &row_rect, 1.0, gray_format, mid_ptr_aux,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
    }

  /* loop rows and compute contrast ratio between both input and aux */    
  for (y = dst_rect->y; y < dst_rect->y + dst_rect->height; y++)
    {
      row_rect.y = y + 1;
      out_rect.y = y;

      gegl_buffer_get (input, &row_rect, 1.0, gray_format, down_ptr_in,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
      if (aux)
        {
          gegl_buffer_get (aux, &row_rect, 1.0, gray_format, down_ptr_aux,
                           GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);
        }

      for (x = 1; x < row_rect.width - 1; x++)
        {
          gfloat dx_in, dx_aux;
          gfloat dy_in, dy_aux;
          gfloat contrast_in, contrast_aux, contrast_offset;
          gfloat YMin = 0.0001;
 
          
          /* contrast of input div by aux */
          gfloat contrast_ratio;
          gfloat luminance_ratio;
          
          /* sum of CIE Y values of neigbouring pixels */
          gdouble YSum_in, YSum_aux;

          /* reciprocal of averaged luminance */
          gdouble recip_avgY_in, recip_avgY_aux;

          /* compute dx, dy and YSum for input and aux buffer */          
          dx_in = (mid_ptr_in[(x-1)] - mid_ptr_in[(x+1)]);
          dy_in = (top_ptr_in[x] - down_ptr_in[x]);
          YSum_in = (mid_ptr_in[(x-1)] + mid_ptr_in[(x+1)] + top_ptr_in[x] + down_ptr_in[x]);
          dx_aux = (mid_ptr_aux[(x-1)] - mid_ptr_aux[(x+1)]);
          dy_aux = (top_ptr_aux[x] - down_ptr_aux[x]);
          YSum_aux = (mid_ptr_aux[(x-1)] + mid_ptr_aux[(x+1)] + top_ptr_aux[x] + down_ptr_aux[x]);

          
          if (fabs(YSum_in) > YMin)
          {
            recip_avgY_in = 4.0 / YSum_in;
            contrast_in = sqrtf (POW2(dx_in) + POW2(dy_in)) * recip_avgY_in * 0.5;
          }
          else
          {
            contrast_in = 0.0;
          }

          if (fabs(YSum_aux) > YMin)
          {
            recip_avgY_aux = 4.0 / YSum_aux;
            contrast_aux = sqrtf (POW2(dx_aux) + POW2(dy_aux)) * recip_avgY_aux * 0.5;
          }
          else
          {
            contrast_aux = 0.0;
          }

          /* computing contrast ratio */                    
          contrast_offset = (contrast_in + contrast_aux) * scale;
          contrast_offset = scale;
          contrast_ratio = (contrast_in + contrast_offset) / (contrast_aux + contrast_offset);

          /* computing luminance ratio */                    
          if (fabs(mid_ptr_aux[x]) > YMin)
          {
            luminance_ratio = mid_ptr_in[x] / mid_ptr_aux[x];
          }
          else
          {
            luminance_ratio = 0.0;
          }
          
          
          
          row_out[(x-1) * 4 + 0] = mid_ptr_in[x];
          row_out[(x-1) * 4 + 1] = mid_ptr_in[x];
          row_out[(x-1) * 4 + 2] = mid_ptr_in[x];
          row_out[(x-1) * 4 + 3] = 1.0; //overwrite alpha
        }

      gegl_buffer_set (output, &out_rect, level, format, row_out,
                       GEGL_AUTO_ROWSTRIDE);

      tmp_ptr_in = top_ptr_in;
      top_ptr_in = mid_ptr_in;
      mid_ptr_in = down_ptr_in;
      down_ptr_in = tmp_ptr_in;

      tmp_ptr_aux = top_ptr_aux;
      top_ptr_aux = mid_ptr_aux;
      mid_ptr_aux = down_ptr_aux;
      down_ptr_aux = tmp_ptr_aux;
    }

  g_free (row1_in);
  g_free (row2_in);
  g_free (row3_in);
  g_free (row1_aux);
  g_free (row2_aux);
  g_free (row3_aux);
  g_free (row_out);
  g_free (in_buf);
  g_free (aux_buf);

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
                          o->scale, o->WhiteRepresentation,
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

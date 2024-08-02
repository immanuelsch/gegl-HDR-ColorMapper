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


#else

#define GEGL_OP_AREA_FILTER
#define GEGL_OP_NAME         image_gradient_rel
#define GEGL_OP_C_SOURCE     image-gradient-rel.c

#define POW2(x) ((x)*(x))

#include "gegl-op.h"

static void
prepare (GeglOperation *operation)
{
  const Babl *space = gegl_operation_get_source_space (operation, "input");
  GeglOperationAreaFilter *area       = GEGL_OPERATION_AREA_FILTER (operation);
  const Babl              *rgb_format = babl_format_with_space ("Y float", space);
  const Babl              *out_format = babl_format_n (babl_type ("float"), 2);

  area->left   =
  area->top    =
  area->right  =
  area->bottom = 1;

  out_format = babl_format_n (babl_type ("float"), 1);

  gegl_operation_set_format (operation, "input",  rgb_format);
  gegl_operation_set_format (operation, "output", out_format);
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

static gboolean
process (GeglOperation       *operation,
         GeglBuffer          *input,
         GeglBuffer          *output,
         const GeglRectangle *roi,
         gint                 level)
{
  const Babl      *in_format  = gegl_operation_get_format (operation, "input");
  const Babl      *out_format = gegl_operation_get_format (operation, "output");
  gfloat *row1;
  gfloat *row2;
  gfloat *row3;
  gfloat *row4;
  gfloat *top_ptr;
  gfloat *mid_ptr;
  gfloat *down_ptr;
  gfloat *tmp_ptr;
  gint    x, y;
  gint    n_components;

  GeglRectangle row_rect;
  GeglRectangle out_rect;

  n_components = babl_format_get_n_components (out_format);
  row1 = g_new (gfloat, (roi->width + 2) );
  row2 = g_new (gfloat, (roi->width + 2) );
  row3 = g_new (gfloat, (roi->width + 2) );
  row4 = g_new0 (gfloat, roi->width * n_components);

  top_ptr  = row1;
  mid_ptr  = row2;
  down_ptr = row3;

  row_rect.width = roi->width + 2;
  row_rect.height = 1;
  row_rect.x = roi->x - 1;
  row_rect.y = roi->y - 1;

  out_rect.x      = roi->x;
  out_rect.width  = roi->width;
  out_rect.height = 1;

  gegl_buffer_get (input, &row_rect, 1.0, in_format, top_ptr,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);

  row_rect.y++;
  gegl_buffer_get (input, &row_rect, 1.0, in_format, mid_ptr,
                   GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);

  for (y = roi->y; y < roi->y + roi->height; y++)
    {
      row_rect.y = y + 1;
      out_rect.y = y;

      gegl_buffer_get (input, &row_rect, 1.0, in_format, down_ptr,
                       GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_CLAMP);

      for (x = 1; x < row_rect.width - 1; x++)
        {
          gfloat dx;
          gfloat dy;
          gfloat magnitude;
          gdouble YSum; // sum of CIE Y values
          gdouble recip_avgY; // reciprocal of averaged luminance

          dx = (mid_ptr[(x-1)] - mid_ptr[(x+1)]);
          dy = (top_ptr[x] - down_ptr[x]);
          YSum = (mid_ptr[(x-1)] + mid_ptr[(x+1)] + top_ptr[x] + down_ptr[x]);

          if (fabs(YSum) > 0.0001)
          {
            recip_avgY = 4.0 / YSum;
//            magnitude = fmax (sqrt (POW2(dx) + POW2(dy)), 0.001) * recip_avgY * 0.5;
            magnitude = sqrt (POW2(dx) + POW2(dy)) * recip_avgY * 0.5;
          }
          else
          {
            magnitude = 0.0;
          }

          row4[(x-1) * n_components] = magnitude;
        }

      gegl_buffer_set (output, &out_rect, level, out_format, row4,
                       GEGL_AUTO_ROWSTRIDE);

      tmp_ptr = top_ptr;
      top_ptr = mid_ptr;
      mid_ptr = down_ptr;
      down_ptr = tmp_ptr;
    }

  g_free (row1);
  g_free (row2);
  g_free (row3);
  g_free (row4);

  return TRUE;
}

static void
gegl_op_class_init (GeglOpClass *klass)
{
  GeglOperationClass       *operation_class;
  GeglOperationFilterClass *filter_class;

  operation_class = GEGL_OPERATION_CLASS (klass);
  filter_class    = GEGL_OPERATION_FILTER_CLASS (klass);

  filter_class->process             = process;
  operation_class->prepare          = prepare;
  operation_class->get_bounding_box = get_bounding_box;
  operation_class->opencl_support   = FALSE;

  gegl_operation_class_set_keys (operation_class,
    "name",        "immanuel:image-gradient-rel",
    "title",       _("image gradient relative"),
    "categories",  "edge-detect",
    "description", _("Compute gradient magnitude "
                     "central differences relative to its lightness"),
    NULL);
}

#endif


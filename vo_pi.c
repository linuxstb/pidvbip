#include <stdio.h>
#include <assert.h>
#include <bcm_host.h>

#include "vo_pi.h"
#include "debug.h"

void vo_open(RECT_VARS_T* vars, int screen)
{
    int ret;

    /* Init from dispmanx.c */

    DEBUGF("Open display[%i]...\n", screen );
    vars->display = vc_dispmanx_display_open( screen );

    ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
    assert(ret == 0);
    DEBUGF( "Display is %d x %d\n", vars->info.width, vars->info.height );

    /* End of dispmanx.c init */
}

void vo_display_frame (RECT_VARS_T* vars, int width, int height,
                      int chroma_width, int chroma_height,
                      uint8_t * const * buf, int num)
{
  int ret;
  static VC_RECT_T       src_rect;
  static VC_RECT_T       dst_rect;
  static VC_DISPMANX_ALPHA_T alpha = { DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS, 
                                255, /*alpha 0->255*/
			        0 };
  int pitch = VO_ALIGN_UP(width,32);

  assert((height % 16) == 0);
  assert((chroma_height % 16) == 0);

  if (num == 0) {
    vars->resource = vc_dispmanx_resource_create( VC_IMAGE_YUV420,
                                                  width,
                                                  height,
                                                  &vars->vc_image_ptr );
    assert( vars->resource );

    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );

    vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16 );

    vc_dispmanx_rect_set( &dst_rect, 0, 0, vars->info.width, vars->info.height);

    vars->element = vc_dispmanx_element_add(    vars->update,
                                                vars->display,
                                                2000,               // layer
                                                &dst_rect,
                                                vars->resource,
                                                &src_rect,
                                                DISPMANX_PROTECTION_NONE,
                                                &alpha,
                                                NULL,             // clamp
                                                VC_IMAGE_ROT0 );
    ret = vc_dispmanx_update_submit( vars->update, NULL, NULL);

    vc_dispmanx_rect_set( &dst_rect, 0, 0, width, (3*height)/2);

  }

  ret = vc_dispmanx_resource_write_data(  vars->resource,
                                          VC_IMAGE_YUV420,
                                          pitch,
                                          buf[0],
                                          &dst_rect );
  assert( ret == 0 );

  vars->update = vc_dispmanx_update_start( 10 );
  assert( vars->update );

  //ret = vc_dispmanx_update_submit_sync( vars->update );
    ret = vc_dispmanx_update_submit( vars->update, NULL, NULL);
    assert( ret == 0 );
}

void vo_close(RECT_VARS_T* vars)
{
    int ret;

    /* Cleanup code from dispmanx.c */
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );

    ret = vc_dispmanx_element_remove( vars->update, vars->element );
    assert( ret == 0 );

    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );

    ret = vc_dispmanx_resource_delete( vars->resource );
    assert( ret == 0 );

    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );
    /* End of cleanup */
}

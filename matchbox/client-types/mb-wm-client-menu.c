#include "mb-wm-client-menu.h"

#include "mb-wm-theme.h"

static Bool
mb_wm_client_menu_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags);

static void
mb_wm_client_menu_realize (MBWindowManagerClient *client)
{
  MBWindowManagerClientClass  *parent_klass = NULL;
  int our_type = MB_WM_TYPE_CLIENT_MENU;

  parent_klass = MB_WM_CLIENT_CLASS (MB_WM_OBJECT_GET_CLASS(client));
  /* Look back down class hierarchy until we find ourself, then
   * find our parent's realize. There must be a better way??? */
  while (MB_WM_OBJECT_CLASS(parent_klass)->type != our_type)
    parent_klass = MB_WM_CLIENT_CLASS(MB_WM_OBJECT_CLASS(parent_klass)->parent);
  parent_klass = MB_WM_CLIENT_CLASS(MB_WM_OBJECT_CLASS(parent_klass)->parent);

  if (parent_klass->realize)
    parent_klass->realize (client);

  /*
   * Must reparent the window to our root, otherwise we restacking of
   * pre-existing windows might fail.
   */
  if (client->xwin_frame)
    XReparentWindow(client->wmref->xdpy, client->xwin_frame,
                    client->wmref->root_win->xwindow, 0, 0);
  else
    XReparentWindow(client->wmref->xdpy, MB_WM_CLIENT_XWIN(client),
                    client->wmref->root_win->xwindow, 0, 0);
}

static void
mb_wm_client_menu_class_init (MBWMObjectClass *klass)
{
  MBWindowManagerClientClass *client;

  MBWM_MARK();

  client = (MBWindowManagerClientClass *)klass;

  client->client_type  = MBWMClientTypeMenu;
  client->geometry     = mb_wm_client_menu_request_geometry;
  client->realize      = mb_wm_client_menu_realize;

#if MBWM_WANT_DEBUG
  klass->klass_name = "MBWMClientMenu";
#endif
}

static void
mb_wm_client_menu_destroy (MBWMObject *this)
{
}

static int
mb_wm_client_menu_init (MBWMObject *this, va_list vap)
{
  MBWindowManagerClient *client      = MB_WM_CLIENT (this);
  MBWindowManager       *wm          = client->wmref;
  MBWMClientWindow      *win         = client->window;
  MBGeometry geom;
  Atom actions[] = {
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_CLOSE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_MOVE],
    wm->atoms[MBWM_ATOM_NET_WM_ACTION_RESIZE],
  };

  XChangeProperty (wm->xdpy, win->xwindow,
		   wm->atoms[MBWM_ATOM_NET_WM_ALLOWED_ACTIONS],
		   XA_ATOM, 32, PropModeReplace,
		   (unsigned char *)actions,
		   sizeof (actions)/sizeof (actions[0]));

  mb_wm_client_set_layout_hints (client,
				 LayoutPrefPositionFree|LayoutPrefVisible|
				 LayoutPrefFixedX|LayoutPrefFixedY);

  if (!client->window->undecorated && wm->theme)
      {
        mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeNorth);
        mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeSouth);
        mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeWest);
        mb_wm_theme_create_decor (wm->theme, client, MBWMDecorTypeEast);
      }

  /*
   * Stack menus on the top of the stacking order, regardless of whether they
   * declare themselves transient or not.
   *
   * (Gtk menus do kbd and pointer grabs and do not take kindly to being
   * restacked.)
   */
#if 0
  if (win->xwin_transient_for
      && win->xwin_transient_for != win->xwindow
      && win->xwin_transient_for != wm->root_win->xwindow)
    {
      MBWM_DBG ("Adding to '%lx' transient list",
		win->xwin_transient_for);
      mb_wm_client_add_transient
	(mb_wm_managed_client_from_xwindow (wm,
					    win->xwin_transient_for),
	 client);
      client->stacking_layer = 0;  /* We stack with whatever transient too */
    }
  else
    {
      MBWM_DBG ("Menu is transient to root");
      /* Stack with 'always on top' */
      client->stacking_layer = MBWMStackLayerTopMid;
    }
#endif

  if (win->hildon_stacking_layer == 0)
    client->stacking_layer = MBWMStackLayerTop;
  else
    client->stacking_layer = win->hildon_stacking_layer
                             + MBWMStackLayerHildon1 - 1;

  geom = client->window->geometry;

  if (MB_WM_CLIENT_CLIENT_TYPE (this) != MBWMClientTypeMenu)
  {
    /* new-style menu */

    geom.y = 0;

    if (wm->xdpy_width > wm->xdpy_height)
      {
	/* Landscape mode: menu has a gutter either side */
	geom.width = wm->xdpy_width - 100 - 16 * 2;
	geom.x = wm->xdpy_width / 2 - geom.width / 2;
      }
    else
      {
	/* Portrait mode */
	geom.width = wm->xdpy_width - 16 * 2;
	geom.x = 16;
      }
  }
  else if (win->hildon_type ==
                wm->atoms[MBWM_ATOM_HILDON_WM_WINDOW_TYPE_LEGACY_MENU])
  {
    /* placement code for legacy application menu, only for the main menu,
     * not for the submenus */
    gint title_x, title_y;

    mb_wm_theme_get_title_xy (wm->theme, &title_x, &title_y);

    if (geom.x < title_x)
      geom.x = title_x;

    if (geom.y < title_y)
      geom.y = title_y;
  }

  if (!client->window->undecorated)
    {
      int n, s, w, e;
      n = s = w = e = 0;

      mb_wm_theme_get_decor_dimensions (wm->theme, client, &n, &s, &w, &e);

      geom.x      -= w;
      geom.y      -= n;
      geom.width  += w + e;
      geom.height += n + s;
    }

  g_debug ("%s: Menu will be at %d %d %d %d", __func__, geom.x, geom.y,
           geom.width, geom.height);

  mb_wm_client_menu_request_geometry (client, &geom,
				      MBWMClientReqGeomForced);

  return 1;
}

int
mb_wm_client_menu_class_type ()
{
  static int type = 0;

  if (UNLIKELY(type == 0))
    {
      static MBWMObjectClassInfo info = {
	sizeof (MBWMClientMenuClass),
	sizeof (MBWMClientMenu),
	mb_wm_client_menu_init,
	mb_wm_client_menu_destroy,
	mb_wm_client_menu_class_init
      };

      type = mb_wm_object_register_class (&info, MB_WM_TYPE_CLIENT_BASE, 0);
    }

  return type;
}

static Bool
mb_wm_client_menu_request_geometry (MBWindowManagerClient *client,
				    MBGeometry            *new_geometry,
				    MBWMClientReqGeomType  flags)
{
  MBWindowManager *wm = client->wmref;
  int north = 0, south = 0, west = 0, east = 0;
  MBGeometry frame_geometry;

  if (client->decor && !client->window->undecorated)
      mb_wm_theme_get_decor_dimensions (wm->theme, client,
                                        &north, &south, &west, &east);

  if (flags & MBWMClientReqGeomIsViaConfigureReq)
    {
      frame_geometry.x        = new_geometry->x - west;
      frame_geometry.y        = new_geometry->y - north;
      frame_geometry.width    = new_geometry->width + (west + east);
      frame_geometry.height   = new_geometry->height + (south + north);
    }
  else
    frame_geometry = *new_geometry;

  /* We don't want to clip geometry for any menus that extend from this
   * like HdAppMenu. In that case, we would wrongly position the menu
   * a few pixels from the edge of the screen.
   */
  if (MB_WM_CLIENT_CLIENT_TYPE (client) == MBWMClientTypeMenu)
    {
      int border = 1;
      // fit to the bottom + right
      if (frame_geometry.x+frame_geometry.width > wm->xdpy_width-border)
        frame_geometry.x = wm->xdpy_width-(border+frame_geometry.width);
      if (frame_geometry.y+frame_geometry.height > wm->xdpy_height-border)
        frame_geometry.y = wm->xdpy_height-(border+frame_geometry.height);
      // fit to the top + left
      if (frame_geometry.x < border)
        frame_geometry.x = border;
      if (frame_geometry.y < border)
        frame_geometry.y = border;
      // finally reduce width and height if we went over
      if (frame_geometry.x+frame_geometry.width > wm->xdpy_width-border)
        frame_geometry.width = wm->xdpy_width-(border+frame_geometry.x);
      if (frame_geometry.y+frame_geometry.height > wm->xdpy_height-border)
        frame_geometry.height = wm->xdpy_height-(border+frame_geometry.y);
    }

  /* Calculate window size from frame */
  client->window->geometry.x      = frame_geometry.x + west;
  client->window->geometry.y      = frame_geometry.y + north;
  client->window->geometry.width  = frame_geometry.width - (west + east);
  client->window->geometry.height = frame_geometry.height - (south + north);
  client->frame_geometry          = frame_geometry;

  mb_wm_client_geometry_mark_dirty (client);

  return True; /* Geometry accepted */
}

MBWindowManagerClient*
mb_wm_client_menu_new (MBWindowManager *wm, MBWMClientWindow *win)
{
  MBWindowManagerClient *client;

  client
    = MB_WM_CLIENT(mb_wm_object_new (MB_WM_TYPE_CLIENT_MENU,
				     MBWMObjectPropWm,           wm,
				     MBWMObjectPropClientWindow, win,
				     NULL));

  return client;
}


/*
 * damagegate: drop raw video frames when the X11 screen has not changed.
 *
 * Neko captures the screen with ximagesrc at a fixed framerate and feeds every
 * frame to videoconvert + the encoder, even when nothing on screen changed.
 * That costs ~half a CPU core for a static 1080p screen.
 *
 * This element sits right after ximagesrc and uses the XDamage extension (no
 * pixel comparison) to decide whether a frame carries anything new:
 *
 *   - screen damage, pointer movement or a cursor shape change  -> pass
 *   - for hold-ms after the last change                          -> pass
 *     (covers the capture/damage race and lets the encoder refine quality)
 *   - a force-key-unit request, up- or downstream (new viewer)   -> pass next,
 *     and request a keyframe for exactly that frame
 *   - at least one frame every max-interval-ms                   -> pass,
 *     as a keyframe (neko relies on periodic keyframes for joining viewers)
 *   - otherwise                                                  -> drop
 *
 * If the X display or the extensions are unavailable it degrades to a plain
 * passthrough, so it can never break the stream.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef PACKAGE
#define PACKAGE "damagegate"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/video.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>

GST_DEBUG_CATEGORY_STATIC(damagegate_debug);
#define GST_CAT_DEFAULT damagegate_debug

#define GST_TYPE_DAMAGE_GATE (gst_damage_gate_get_type())
G_DECLARE_FINAL_TYPE(GstDamageGate, gst_damage_gate, GST, DAMAGE_GATE, GstBaseTransform)

struct _GstDamageGate {
  GstBaseTransform parent;

  /* properties */
  gchar *display_name;
  guint max_interval_ms;
  guint hold_ms;
  guint keyframe_interval_ms;
  gboolean track_pointer;
  gboolean enabled;

  /* X11 state (streaming thread only) */
  Display *dpy;
  Window root;
  Damage damage;
  int damage_event_base;
  int fixes_event_base;
  gboolean have_fixes;
  int last_x, last_y;

  /* gate state */
  gint64 last_pass_us;
  gint64 last_kf_us;
  GstClockTime last_pass_pts;
  gint64 last_change_us;
  gint force_pass; /* atomic */

  /* stats */
  guint64 passed;
  guint64 dropped;
  gint64 last_stats_us;
};

enum {
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_MAX_INTERVAL_MS,
  PROP_HOLD_MS,
  PROP_KEYFRAME_INTERVAL_MS,
  PROP_TRACK_POINTER,
  PROP_ENABLED,
  PROP_PASSED,
  PROP_DROPPED,
};

#define DEFAULT_MAX_INTERVAL_MS 1000
#define DEFAULT_HOLD_MS 200
#define DEFAULT_KEYFRAME_INTERVAL_MS 1000

static GstStaticPadTemplate sink_template =
    GST_STATIC_PAD_TEMPLATE("sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw"));
static GstStaticPadTemplate src_template =
    GST_STATIC_PAD_TEMPLATE("src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("video/x-raw"));

G_DEFINE_TYPE(GstDamageGate, gst_damage_gate, GST_TYPE_BASE_TRANSFORM)

static void close_display(GstDamageGate *self) {
  if (self->dpy) {
    if (self->damage) XDamageDestroy(self->dpy, self->damage);
    XCloseDisplay(self->dpy);
  }
  self->dpy = NULL;
  self->damage = 0;
  self->have_fixes = FALSE;
}

static gboolean gst_damage_gate_start(GstBaseTransform *trans) {
  GstDamageGate *self = GST_DAMAGE_GATE(trans);
  int err_base, major = 1, minor = 1;

  self->passed = self->dropped = 0;
  self->last_pass_us = 0;
  self->last_kf_us = 0;
  self->last_pass_pts = GST_CLOCK_TIME_NONE;
  self->last_change_us = g_get_monotonic_time();
  self->last_stats_us = self->last_change_us;
  self->last_x = self->last_y = -1;
  g_atomic_int_set(&self->force_pass, 1);

  self->dpy = XOpenDisplay(self->display_name);
  if (!self->dpy) {
    GST_ELEMENT_WARNING(self, RESOURCE, OPEN_READ, ("damagegate: cannot open X display, passing all frames"), (NULL));
    return TRUE;
  }

  if (!XDamageQueryExtension(self->dpy, &self->damage_event_base, &err_base) ||
      !XDamageQueryVersion(self->dpy, &major, &minor)) {
    GST_ELEMENT_WARNING(self, RESOURCE, OPEN_READ, ("damagegate: XDamage not available, passing all frames"), (NULL));
    close_display(self);
    return TRUE;
  }

  self->root = DefaultRootWindow(self->dpy);
  self->damage = XDamageCreate(self->dpy, self->root, XDamageReportNonEmpty);

  if (XFixesQueryExtension(self->dpy, &self->fixes_event_base, &err_base)) {
    XFixesSelectCursorInput(self->dpy, self->root, XFixesDisplayCursorNotifyMask);
    self->have_fixes = TRUE;
  }

  XSync(self->dpy, False);
  GST_INFO_OBJECT(self, "watching X damage on %s (fixes=%d)", DisplayString(self->dpy), self->have_fixes);
  return TRUE;
}

static gboolean gst_damage_gate_stop(GstBaseTransform *trans) {
  GstDamageGate *self = GST_DAMAGE_GATE(trans);
  GST_INFO_OBJECT(self, "stopping: passed=%" G_GUINT64_FORMAT " dropped=%" G_GUINT64_FORMAT, self->passed,
                  self->dropped);
  close_display(self);
  return TRUE;
}

/* Returns TRUE if anything visible changed since the previous call. */
static gboolean poll_changes(GstDamageGate *self, int *why) {
  gboolean changed = FALSE;
  gboolean damaged = FALSE;

  if (self->track_pointer) {
    Window r, c;
    int rx, ry, wx, wy;
    unsigned int mask;
    /* round trip: also flushes and pulls in all pending events */
    if (XQueryPointer(self->dpy, self->root, &r, &c, &rx, &ry, &wx, &wy, &mask)) {
      if (rx != self->last_x || ry != self->last_y) {
        changed = TRUE;
        *why |= 1;
        self->last_x = rx;
        self->last_y = ry;
      }
    }
  } else {
    XSync(self->dpy, False);
  }

  while (XPending(self->dpy)) {
    XEvent ev;
    XNextEvent(self->dpy, &ev);
    if (ev.type == self->damage_event_base + XDamageNotify) {
      damaged = TRUE;
    } else if (self->have_fixes && ev.type == self->fixes_event_base + XFixesCursorNotify) {
      changed = TRUE;
      *why |= 2;
    }
  }

  if (damaged) {
    /* reset the damage region so the next change produces a new notify */
    if (gst_debug_category_get_threshold(damagegate_debug) >= GST_LEVEL_TRACE && self->have_fixes) {
      XserverRegion parts = XFixesCreateRegion(self->dpy, NULL, 0);
      XDamageSubtract(self->dpy, self->damage, None, parts);
      int n = 0;
      XRectangle bounds;
      XRectangle *r = XFixesFetchRegionAndBounds(self->dpy, parts, &n, &bounds);
      GST_TRACE_OBJECT(self, "damage: %d rects, bounds %dx%d+%d+%d", n, bounds.width, bounds.height, bounds.x, bounds.y);
      if (r) XFree(r);
      XFixesDestroyRegion(self->dpy, parts);
    } else {
      XDamageSubtract(self->dpy, self->damage, None, None);
    }
    XFlush(self->dpy);
    changed = TRUE;
    *why |= 4;
  }

  return changed;
}

static void request_keyframe(GstDamageGate *self, GstBaseTransform *trans, GstBuffer *buf, gint64 now) {
  GstClockTime pts = GST_BUFFER_PTS(buf);
  GstClockTime rt = gst_segment_to_running_time(&trans->segment, GST_FORMAT_TIME, pts);
  GstClockTime st = gst_segment_to_stream_time(&trans->segment, GST_FORMAT_TIME, pts);
  gst_pad_push_event(GST_BASE_TRANSFORM_SRC_PAD(trans), gst_video_event_new_downstream_force_key_unit(pts, st, rt, TRUE, 0));
  self->last_kf_us = now;
}

static GstFlowReturn gst_damage_gate_transform_ip(GstBaseTransform *trans, GstBuffer *buf) {
  GstDamageGate *self = GST_DAMAGE_GATE(trans);
  gint64 now;
  gboolean pass = FALSE;

  if (!self->enabled || !self->dpy) return GST_FLOW_OK; /* plain passthrough */

  now = g_get_monotonic_time();

  int why = 0;
  if (poll_changes(self, &why)) self->last_change_us = now;
  if (why) GST_LOG_OBJECT(self, "change: pointer=%d cursor=%d damage=%d", !!(why & 1), !!(why & 2), !!(why & 4));

  if (g_atomic_int_compare_and_exchange(&self->force_pass, 1, 0)) {
    /* New viewer: neko's own keyframe request carries the running time of the
     * moment it was made; if the matching frame is dropped the encoder waits for
     * its periodic keyframe. Pin a keyframe to this frame instead. */
    pass = TRUE;
    request_keyframe(self, trans, buf, now);
  } else if (now - self->last_change_us <= (gint64)self->hold_ms * 1000) {
    pass = TRUE;
  } else if (self->max_interval_ms > 0 && now - self->last_pass_us >= (gint64)self->max_interval_ms * 1000) {
    /* Heartbeat on a static screen. Encoders count keyframe distance in frames,
     * so at ~1 fps "every 25 frames" becomes every 25 s, and neko does not
     * forward receiver PLIs to the encoder. Make heartbeats keyframes (time
     * based) so a joining or recovering viewer gets a picture within ~1 s. */
    pass = TRUE;
    if (self->keyframe_interval_ms > 0 && now - self->last_kf_us >= (gint64)self->keyframe_interval_ms * 1000)
      request_keyframe(self, trans, buf, now);
  }

  if (now - self->last_stats_us >= 60 * G_USEC_PER_SEC) {
    GST_DEBUG_OBJECT(self, "passed=%" G_GUINT64_FORMAT " dropped=%" G_GUINT64_FORMAT, self->passed, self->dropped);
    self->last_stats_us = now;
  }

  if (pass) {
    /* Stretch the duration over the dropped frames. Neko turns buffer durations
     * into RTP timestamp increments; without this the receiver's timeline would
     * drift behind wall clock by every dropped frame. */
    GstClockTime pts = GST_BUFFER_PTS(buf);
    if (gst_mini_object_is_writable(GST_MINI_OBJECT_CAST(buf)) && GST_CLOCK_TIME_IS_VALID(pts) &&
        GST_CLOCK_TIME_IS_VALID(self->last_pass_pts) && pts > self->last_pass_pts) {
      GstClockTime gap = pts - self->last_pass_pts;
      if (!GST_BUFFER_DURATION_IS_VALID(buf) || gap > GST_BUFFER_DURATION(buf)) GST_BUFFER_DURATION(buf) = gap;
    }
    self->last_pass_pts = pts;
    self->last_pass_us = now;
    self->passed++;
    return GST_FLOW_OK;
  }

  self->dropped++;
  return GST_BASE_TRANSFORM_FLOW_DROPPED;
}

static gboolean gst_damage_gate_src_event(GstBaseTransform *trans, GstEvent *event) {
  GstDamageGate *self = GST_DAMAGE_GATE(trans);

  /* encoder asks for a keyframe (new viewer / PLI): make sure a frame reaches it */
  if (gst_video_event_is_force_key_unit(event)) {
    GST_DEBUG_OBJECT(self, "force-key-unit requested, passing next frame");
    g_atomic_int_set(&self->force_pass, 1);
  }

  return GST_BASE_TRANSFORM_CLASS(gst_damage_gate_parent_class)->src_event(trans, event);
}

static gboolean gst_damage_gate_sink_event(GstBaseTransform *trans, GstEvent *event) {
  GstDamageGate *self = GST_DAMAGE_GATE(trans);

  /* neko sends its keyframe request downstream from the pipeline (new listener) */
  if (gst_video_event_is_force_key_unit(event)) {
    GST_DEBUG_OBJECT(self, "downstream force-key-unit, passing next frame");
    g_atomic_int_set(&self->force_pass, 1);
  }

  return GST_BASE_TRANSFORM_CLASS(gst_damage_gate_parent_class)->sink_event(trans, event);
}

static void gst_damage_gate_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
  GstDamageGate *self = GST_DAMAGE_GATE(object);
  switch (prop_id) {
    case PROP_DISPLAY_NAME:
      g_free(self->display_name);
      self->display_name = g_value_dup_string(value);
      break;
    case PROP_MAX_INTERVAL_MS:
      self->max_interval_ms = g_value_get_uint(value);
      break;
    case PROP_HOLD_MS:
      self->hold_ms = g_value_get_uint(value);
      break;
    case PROP_KEYFRAME_INTERVAL_MS:
      self->keyframe_interval_ms = g_value_get_uint(value);
      break;
    case PROP_TRACK_POINTER:
      self->track_pointer = g_value_get_boolean(value);
      break;
    case PROP_ENABLED:
      self->enabled = g_value_get_boolean(value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void gst_damage_gate_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
  GstDamageGate *self = GST_DAMAGE_GATE(object);
  switch (prop_id) {
    case PROP_DISPLAY_NAME:
      g_value_set_string(value, self->display_name);
      break;
    case PROP_MAX_INTERVAL_MS:
      g_value_set_uint(value, self->max_interval_ms);
      break;
    case PROP_HOLD_MS:
      g_value_set_uint(value, self->hold_ms);
      break;
    case PROP_KEYFRAME_INTERVAL_MS:
      g_value_set_uint(value, self->keyframe_interval_ms);
      break;
    case PROP_TRACK_POINTER:
      g_value_set_boolean(value, self->track_pointer);
      break;
    case PROP_ENABLED:
      g_value_set_boolean(value, self->enabled);
      break;
    case PROP_PASSED:
      g_value_set_uint64(value, self->passed);
      break;
    case PROP_DROPPED:
      g_value_set_uint64(value, self->dropped);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
  }
}

static void gst_damage_gate_finalize(GObject *object) {
  GstDamageGate *self = GST_DAMAGE_GATE(object);
  close_display(self);
  g_free(self->display_name);
  G_OBJECT_CLASS(gst_damage_gate_parent_class)->finalize(object);
}

static void gst_damage_gate_class_init(GstDamageGateClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseTransformClass *trans_class = GST_BASE_TRANSFORM_CLASS(klass);

  gobject_class->set_property = gst_damage_gate_set_property;
  gobject_class->get_property = gst_damage_gate_get_property;
  gobject_class->finalize = gst_damage_gate_finalize;

  g_object_class_install_property(
      gobject_class, PROP_DISPLAY_NAME,
      g_param_spec_string("display-name", "Display", "X display to watch (default: $DISPLAY)", NULL,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_MAX_INTERVAL_MS,
      g_param_spec_uint("max-interval-ms", "Max interval", "Pass at least one frame every N ms (0 = never)", 0,
                        G_MAXUINT, DEFAULT_MAX_INTERVAL_MS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_HOLD_MS,
      g_param_spec_uint("hold-ms", "Hold", "Keep passing frames for N ms after the last change", 0, G_MAXUINT,
                        DEFAULT_HOLD_MS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_KEYFRAME_INTERVAL_MS,
      g_param_spec_uint("keyframe-interval-ms", "Keyframe interval",
                        "While the screen is static, make heartbeat frames keyframes at most every N ms (0 = off)", 0,
                        G_MAXUINT, DEFAULT_KEYFRAME_INTERVAL_MS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_TRACK_POINTER,
      g_param_spec_boolean("track-pointer", "Track pointer", "Treat pointer movement as a change", TRUE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_ENABLED,
      g_param_spec_boolean("enabled", "Enabled", "Drop unchanged frames (FALSE = passthrough)", TRUE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_PASSED,
      g_param_spec_uint64("passed", "Passed", "Frames passed", 0, G_MAXUINT64, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property(
      gobject_class, PROP_DROPPED,
      g_param_spec_uint64("dropped", "Dropped", "Frames dropped", 0, G_MAXUINT64, 0,
                          G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata(element_class, "X11 damage gate", "Filter/Video",
                                        "Drops raw video frames when the X11 screen has not changed",
                                        "neko-chrome-cdp-docker");
  gst_element_class_add_static_pad_template(element_class, &sink_template);
  gst_element_class_add_static_pad_template(element_class, &src_template);

  trans_class->start = GST_DEBUG_FUNCPTR(gst_damage_gate_start);
  trans_class->stop = GST_DEBUG_FUNCPTR(gst_damage_gate_stop);
  trans_class->transform_ip = GST_DEBUG_FUNCPTR(gst_damage_gate_transform_ip);
  trans_class->src_event = GST_DEBUG_FUNCPTR(gst_damage_gate_src_event);
  trans_class->sink_event = GST_DEBUG_FUNCPTR(gst_damage_gate_sink_event);
  trans_class->transform_ip_on_passthrough = TRUE;
}

static void gst_damage_gate_init(GstDamageGate *self) {
  self->max_interval_ms = DEFAULT_MAX_INTERVAL_MS;
  self->hold_ms = DEFAULT_HOLD_MS;
  self->keyframe_interval_ms = DEFAULT_KEYFRAME_INTERVAL_MS;
  self->track_pointer = TRUE;
  self->enabled = TRUE;
  /* Passthrough: ximagesrc memory is NO_SHARE, so asking for a writable buffer
   * would deep-copy every 8 MB frame. We only touch the duration field, and
   * only when we hold the sole reference. */
  gst_base_transform_set_passthrough(GST_BASE_TRANSFORM(self), TRUE);
  gst_base_transform_set_in_place(GST_BASE_TRANSFORM(self), TRUE);
}

static gboolean plugin_init(GstPlugin *plugin) {
  GST_DEBUG_CATEGORY_INIT(damagegate_debug, "damagegate", 0, "X11 damage gate");
  return gst_element_register(plugin, "damagegate", GST_RANK_NONE, GST_TYPE_DAMAGE_GATE);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, damagegate, "Drop unchanged X11 frames", plugin_init, "1.0",
                  "MIT", "neko-chrome-cdp-docker", "https://github.com/frankhommers/neko-chrome-cdp-docker")

#include <libxml/xmlreader.h>
#include <libgupnp/gupnp-control-point.h>
#include <libnotify/notify.h>
#include <string.h>
#include <time.h>

typedef struct {
  char *class;
  char *title;
  char *artist;
} DIDL;

static void
scrob (DIDL *didl)
{
  const char *type;
  char *command;
  int res;
  
  if (strcmp (didl->class, "object.item.audioItem.audioBroadcast") == 0)
    type = "R";
  else
    type = "P";

  command = g_strdup_printf ("dbus-send --print-reply --dest=com.burtonini.Scrobbler "
                             "/com/burtonini/Scrobbler "
                             "com.burtonini.Scrobbler.Submit "
                             "uint32:%ld " /* timestamp */
                             "string:\"%s\" " /* artist */
                             "string:\"%s\" " /* title */
                             "uint32:0 " /* track number */
                             "uint32:0 " /* length */
                             "string: " /* album */
                             "string: " /* musicbrainz ID */
                             "string:%s", /* type */
                             time (NULL), didl->artist, didl->title, type);
  res = system (command);
  g_free (command);
}

static void
notify (DIDL *didl)
{
  GError *error = NULL;
  NotifyNotification *notify;
  char *message;

  if (didl->title && didl->artist)
    message = g_markup_printf_escaped ("Playing %s by %s", didl->title, didl->artist);
  else if (didl->title && !didl->artist)
    message = g_markup_printf_escaped ("Playing %s", didl->title);
  else if (!didl->title && didl->artist)
    message = g_markup_printf_escaped ("Playing %s", didl->artist);
  else
    return;
  
  /* TODO: if the notify is already on screen, change it */
  
  notify = notify_notification_new (message, NULL,
                                    "audio-volume-high", NULL);
  
  notify_notification_set_urgency (notify, NOTIFY_URGENCY_LOW);
  
  if (!notify_notification_show (notify, &error)) {
    g_warning ("Cannot show notification: %s", error->message);
    g_error_free (error);
  }
  
  g_object_unref (notify);
  
  g_free (message);
}

static const struct {
  const char *node;
  int offset;
} map[] = {
  { "upnp:class", G_STRUCT_OFFSET (DIDL, class) },
  { "dc:title", G_STRUCT_OFFSET (DIDL, title) },
  { "upnp:artist", G_STRUCT_OFFSET (DIDL, artist) },
};

static void
parse_meta (const xmlChar *xml)
{
  DIDL didl = { NULL, NULL, NULL };
  xmlTextReader *reader;
  int i, res;

  reader = xmlReaderForDoc (xml, NULL, NULL, 0);
  
  while ((res = xmlTextReaderRead (reader)) == 1) {
    const xmlChar *tagname;

    if (xmlTextReaderNodeType (reader) != XML_READER_TYPE_ELEMENT)
      continue;

    tagname = xmlTextReaderConstName (reader);
    
    for (i = 0; i < G_N_ELEMENTS (map); i++) {
      if (xmlStrcmp (tagname, (xmlChar*)map[i].node) != 0)
        continue;

      xmlTextReaderRead (reader);

      G_STRUCT_MEMBER (xmlChar*, &didl, map[i].offset) = xmlTextReaderValue (reader);
    }
  }
  xmlFreeTextReader (reader);

  if (didl.title || didl.artist) {
    notify (&didl);
    scrob (&didl);
  }

  g_free (didl.class);
  g_free (didl.title);
  g_free (didl.artist);
}

static void
notify_cb (GUPnPServiceProxy *proxy,
           const char        *variable,
           GValue            *value,
           gpointer           user_data)
{
  const char *xml;
  xmlTextReader *reader;
  int res;
  xmlChar *val;

  xml = g_value_get_string (value);

  reader = xmlReaderForMemory (xml, strlen (xml), NULL, NULL, 0);
  
  while ((res = xmlTextReaderRead (reader)) == 1) {
    if (xmlTextReaderNodeType (reader) == XML_READER_TYPE_ELEMENT &&
        xmlStrcmp (xmlTextReaderConstName (reader), (xmlChar*)"CurrentTrackMetaData") == 0) {
      val = xmlTextReaderGetAttribute (reader, (xmlChar*)"val");
      parse_meta (val);
      g_free (val);
      break;
    }
  }
  xmlFreeTextReader (reader);
}

static void
service_proxy_available_cb (GUPnPControlPoint *cp,
                            GUPnPServiceProxy *proxy)
{
        gupnp_service_proxy_add_notify (proxy,
                                        "LastChange",
                                        G_TYPE_STRING,
                                        notify_cb,
                                        NULL);

        gupnp_service_proxy_set_subscribed (proxy, TRUE);
}

int
main (int argc, char **argv)
{
        GMainLoop *main_loop;
        GError *error;
        GUPnPContext *context;
        GUPnPControlPoint *cp;

        g_thread_init (NULL);
        g_type_init ();
        notify_init ("tracknotify");
 
        error = NULL;
        context = gupnp_context_new (NULL, "192.168.1.65", 0, &error);
        if (error) {
                g_error (error->message);
                g_error_free (error);

                return 1;
        }

        cp = gupnp_control_point_new
          (context, "urn:schemas-upnp-org:service:AVTransport:1");

        g_signal_connect (cp,
                          "service-proxy-available",
                          G_CALLBACK (service_proxy_available_cb),
                          NULL);
        
        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}

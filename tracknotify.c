#include <libxml/xmlreader.h>
#include <libgupnp/gupnp-control-point.h>
#include <libnotify/notify.h>
#include <string.h>

static void
subscription_lost_cb (GUPnPServiceProxy *proxy,
                      const GError      *reason,
                      gpointer           user_data)
{
        g_print ("Lost subscription: %s\n", reason->message);
}

static void
notify (const char *title)
{
  NotifyNotification *notify;
  char *message;

  if (title == NULL)
    return;

  message = g_strdup_printf ("Playing %s", title);

  notify = notify_notification_new (message, NULL,
                                    "audio-volume-high", NULL);
  
  notify_notification_set_urgency (notify, NOTIFY_URGENCY_LOW);
  
  notify_notification_show (notify, NULL);

  g_object_unref (notify);
  
  g_free (message);
}

static void
parse_meta (const xmlChar *xml)
{
  xmlTextReader *reader;
  int res;
  
  reader = xmlReaderForDoc (xml, NULL, NULL, 0);
  
  while ((res = xmlTextReaderRead (reader)) == 1) {
    if (xmlTextReaderNodeType (reader) == XML_READER_TYPE_ELEMENT &&
        xmlStrcmp (xmlTextReaderConstName (reader), (xmlChar*)"dc:title") == 0) {
      xmlTextReaderRead (reader);
      notify ((char*)xmlTextReaderConstValue (reader));
      break;
    }
  }
  xmlFreeTextReader (reader);
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
        const char *location;

        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("AVTransport available:\n");
        g_print ("\tlocation: %s\n", location);

        gupnp_service_proxy_add_notify (proxy,
                                        "LastChange",
                                        G_TYPE_STRING,
                                        notify_cb,
                                        NULL);

        /* Subscribe */
        g_signal_connect (proxy,
                          "subscription-lost",
                          G_CALLBACK (subscription_lost_cb),
                          NULL);

        gupnp_service_proxy_set_subscribed (proxy, TRUE);
}

static void
service_proxy_unavailable_cb (GUPnPControlPoint *cp,
                              GUPnPServiceProxy *proxy)
{
        const char *location;

        location = gupnp_service_info_get_location (GUPNP_SERVICE_INFO (proxy));

        g_print ("AVTransport unavailable:\n");
        g_print ("\tlocation: %s\n", location);
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
        g_signal_connect (cp,
                          "service-proxy-unavailable",
                          G_CALLBACK (service_proxy_unavailable_cb),
                          NULL);

        gssdp_resource_browser_set_active (GSSDP_RESOURCE_BROWSER (cp), TRUE);

        main_loop = g_main_loop_new (NULL, FALSE);

        g_main_loop_run (main_loop);
        g_main_loop_unref (main_loop);

        g_object_unref (cp);
        g_object_unref (context);

        return 0;
}

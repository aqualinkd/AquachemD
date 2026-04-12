
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "mongoose.h"

#include "aquachemd.h"
#include "utils.h"
#include "config.h"
#include "net_services.h"
#include "mqtt_discovery.h"
#include "net_interface.h"

static struct aquachemdata *_aquachemd_data;
//static char *_web_root;

static pthread_t _net_thread_id = 0;
static bool _keepNetServicesRunning = false;
static struct mg_mgr _mgr;
static int _mqtt_exit_flag = false;

static struct mg_http_serve_opts _http_server_opts;
static struct mg_http_serve_opts _http_server_opts_nocache;

void reset_last_mqtt_status();
void broadcast_aquachemdstate(struct mg_connection *nc);
void start_mqtt(struct mg_mgr *mgr);


#define FAST_SUFFIX_3_CI(str, len, SUFFIX) ( \
    (len) >= 3 && \
    tolower((unsigned char)(str)[(len)-3]) == tolower((unsigned char)(SUFFIX)[0]) && \
    tolower((unsigned char)(str)[(len)-2]) == tolower((unsigned char)(SUFFIX)[1]) && \
    tolower((unsigned char)(str)[(len)-1]) == tolower((unsigned char)(SUFFIX)[2]) \
)

struct mg_connection *mg_next(struct mg_mgr *s, struct mg_connection *conn) {
  return conn == NULL ? s->conns : conn->next;
}

static void net_signal_handler(int sig_num) {
  intHandler(sig_num); // Force signal handler to aquachemd.c
}

static void mg_logger(char ch, void *param) {
  
  static char buf[256];
  static size_t len;
  buf[len++] = ch;
  if (ch == '\n' || len >= sizeof(buf)) {
    //syslog(LOG_INFO, "%.*s", (int) len, buf); // Send logs
    LOG(LOG_INFO, buf);
    len = 0;
    memset(buf, 0, sizeof(buf));
  }
}

static int is_websocket(const struct mg_connection *nc) {
  //return nc->flags & MG_F_IS_WEBSOCKET && !(nc->flags & MG_F_USER_2); // WS only, not WS simulator
  //return nc->flags & MG_F_IS_WEBSOCKET;
  return nc->is_websocket;
}

static void set_websocket_acdmanager(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_WS_AQM; 
}
static int is_websocket_acdmanager(const struct mg_connection *nc) {
  return nc->aq_flags & AQ_MG_CON_WS_AQM;
}
static int is_mqtt(const struct mg_connection *nc) {
  return nc->aq_flags & (AQ_MG_CON_MQTT | AQ_MG_CON_MQTT_CONNECTING);
}
static int is_mqttconnecting(const struct mg_connection *nc) {
  return nc->aq_flags & AQ_MG_CON_MQTT_CONNECTING;
}
static void set_mqttconnecting(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_MQTT_CONNECTING; 
}
static void set_mqttconnected(struct mg_connection *nc) {
  nc->aq_flags |= AQ_MG_CON_MQTT;
  nc->aq_flags &= ~AQ_MG_CON_MQTT_CONNECTING;
}

void log_mg_str(int level, char *name, struct mg_str str) {
  char buf[256];
  size_t len = str.len < sizeof(buf) - 1 ? str.len : sizeof(buf) - 1;
  memcpy(buf, str.buf, len);
  buf[len] = '\0';
  LOG(level, "%s: %s", name, buf);
}

void send_mqtt(struct mg_connection *nc, const char *toppic, const char *message)
{
  //static uint16_t msg_id = 0;

  if (toppic == NULL)
    return;

  struct mg_mqtt_opts pub_opts = {.topic = mg_str(toppic),
                                .message = mg_str(message),
                                .qos = 1,
                                .retain = true};
  uint16_t msg_id = mg_mqtt_pub(nc, &pub_opts);

  LOG(LOG_INFO, "MQTT: Published id=%d: %s %s\n", msg_id, toppic, message);
}



void action_web_request(struct mg_connection *nc, struct mg_http_message *http_msg)
{
  char *msg = NULL;

  log_mg_str(LOG_DEBUG, "URI request", http_msg->uri);
  log_mg_str(LOG_DEBUG, "Query request", http_msg->query);
}

void action_websocket_request(struct mg_connection *nc, struct mg_ws_message *wm) 
{
  log_mg_str(LOG_DEBUG, "Websocket message", wm->data);
}

void action_mqtt_message(struct mg_connection *nc, struct mg_mqtt_message *msg) 
{
  log_mg_str(LOG_DEBUG, "MQTT message topic", msg->topic);
  log_mg_str(LOG_DEBUG, "MQTT message payload", msg->data);
}

void action_mqtt_condition_message(acd_condition *condition, struct mg_mqtt_message *mqtt_msg)
{
  LOG(LOG_INFO, "MQTT: Received message for condition '%s': %.*s\n", condition->label, mqtt_msg->data.len, mqtt_msg->data.buf);
  
  if (strncmp(mqtt_msg->data.buf, condition->mqtt_value, mqtt_msg->data.len) == 0) {
    LOG(LOG_INFO, "MQTT: Condition '%s' met\n", condition->label);
    SET_IF_CHANGED(condition->met, true, _aquachemd_data->is_dirty); // Mark data as dirty so it gets sent to clients right away instead of waiting for next sensor read. 
  } else {
    LOG(LOG_INFO, "MQTT: Condition '%s' not met\n", condition->label);
    SET_IF_CHANGED(condition->met, false, _aquachemd_data->is_dirty);
  }
}

void action_mqtt_sensor_message(acd_key_t *sensor, struct mg_mqtt_message *mqtt_msg)
{
  LOG(LOG_INFO, "MQTT: Received message for sensor '%s': %.*s\n", sensor->label, mqtt_msg->data.len, mqtt_msg->data.buf);
  
  if (sensor->type == KEY_TYPE_MQTT_TEMP) {
    float new_value = strtof(mqtt_msg->data.buf, NULL);
    LOG(LOG_INFO, "MQTT: Updating sensor '%s' value to %.2f\n", sensor->label, new_value);
    SET_IF_CHANGED(sensor->value, new_value, _aquachemd_data->is_dirty); // Mark data as dirty so it gets sent to clients right away instead of waiting for next sensor read. 
  } else {
    LOG(LOG_WARNING, "MQTT: Received message for unsupported sensor type for sensor '%s'\n", sensor->label);
  }
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct mg_mqtt_message *mqtt_msg;
  struct mg_http_message *http_msg;
  struct mg_ws_message *ws_msg;
  char aq_topic[30];
  #ifdef AQ_TM_DEBUG 
    int tid; 
  #endif
  //static double last_control_time;

  // LOG(LOG_DEBUG, "Event\n");
  switch (ev) {
  //case MG_EV_HTTP_REQUEST:
  case MG_EV_HTTP_MSG:
    http_msg = (struct mg_http_message *)ev_data;

    //if ( strstr(http_msg->head.buf, "Upgrade: websocket")  ) {
    if ( mg_http_get_header(http_msg, "Sec-WebSocket-Key") != NULL) {
      LOG(LOG_DEBUG, "Enable websockets\n");
      mg_ws_upgrade(nc, http_msg, NULL);
      break;
    }

    action_web_request(nc, http_msg);
    LOG(LOG_DEBUG, "Served WEB request\n");
    break;
  
  case MG_EV_WS_OPEN:
    _aquachemd_data->open_websockets++;
    LOG(LOG_DEBUG, "++ Websocket joined\n");
    break;
  
  case MG_EV_WS_MSG:
    ws_msg = (struct mg_ws_message *)ev_data;
    action_websocket_request(nc, ws_msg);
    break;
  
  case MG_EV_CLOSE: 
    if (is_websocket(nc)) {
      _aquachemd_data->open_websockets--;
      LOG(LOG_DEBUG, "-- Websocket left\n");
      if (is_websocket_acdmanager(nc)) {
        _aquachemd_data->acdManagerActive = false;
        LOG(LOG_DEBUG, "Stoped Aquachemd Manager\n");
      }
    } else if (is_mqtt(nc) || is_mqttconnecting(nc) ) {
      LOG(LOG_WARNING, "MQTT Connection closed\n");
      _mqtt_exit_flag = true;
    }

    break;
  
  case MG_EV_ACCEPT: 
    if (is_mqtt(nc)) {
      return;
    }
    // Only want HTTPS & WS connections
#if MG_TLS > 0
    if (nc->is_tls) {
      static char *crt;
      static char *key; 
      static char *ca;
      
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));

      if (crt == NULL || key == NULL) {
        LOG(LOG_NOTICE, "HTTPS: loading certs from : %s\n", _acdconfig_.cert_dir);
        crt = read_pem_file(false, "%s/crt.pem",_acdconfig_.cert_dir);
        key = read_pem_file(false, "%s/key.pem",_acdconfig_.cert_dir);
        ca = read_pem_file(true, "%s/ca.pem",_acdconfig_.cert_dir); // If this doesn't exist we don't care. If it exists, 2 way auth
      }
      opts.ca = mg_str(ca);    // Most cases this will be null, only get's set for 2 way auth (ie load cert and authority onto client)
      opts.cert = mg_str(crt);
      opts.key = mg_str(key);      
      mg_tls_init(nc, &opts);
    }
#endif
    break;

  case MG_EV_CONNECT: {
    set_mqttconnected(nc);
    //set_mqtt(nc);
    _mqtt_exit_flag = false;
    LOG(LOG_DEBUG, "MQTT: Connected to : %s\n", _acdconfig_.mqtt_server);
#if MG_TLS > 0
    if (nc->is_tls) {
      static char *crt;
      static char *key;
      static char *ca;
      
      struct mg_tls_opts opts;
      memset(&opts, 0, sizeof(opts));

      if (crt == NULL || key == NULL) {
        LOG(LOG_NOTICE, "MQTTS: loading certs from : %s\n", _acdconfig_.mqtt_cert_dir);
        crt = read_pem_file(false, "%s/crt.pem",_acdconfig_.cert_dir);
        key = read_pem_file(false, "%s/key.pem",_acdconfig_.cert_dir);
        ca = read_pem_file(true, "%s/ca.pem",_acdconfig_.cert_dir);
      }
      opts.cert = mg_str(crt);
      opts.key = mg_str(key);
      opts.ca = mg_str(ca);
      mg_tls_init(nc, &opts);
    }
#endif
  } break;

  case MG_EV_MQTT_OPEN:
    {
      //struct mg_mqtt_opts sub_opts
      static uint8_t qos=0;// PUT IN FUNCTION HEADDER can't be bothered with ack, so set to 0

      LOG(LOG_DEBUG, "MQTT: Connection open %lu\n", nc->id);

      snprintf(aq_topic, 29, "%s/#", _acdconfig_.mqtt_aquachemd_topic);
      //mqtt_subscribe(nc, aq_topic);
      struct mg_mqtt_opts sub_opts;
      memset(&sub_opts, 0, sizeof(sub_opts));
      sub_opts.topic = mg_str(aq_topic);
      sub_opts.qos = qos;
      LOG(LOG_INFO, "MQTT: Subscribing to '%s'\n", aq_topic);
      mg_mqtt_sub(nc, &sub_opts);

      // Any MQTT conditions we need to subscribe to?
      for (acd_condition *curr = _aquachemd_data->conditions; curr != NULL; curr = curr->next) {
        if (curr->type == COND_MQTT) {
          memset(&sub_opts, 0, sizeof(sub_opts));
          sub_opts.topic = mg_str(curr->mqtt_topic);
          sub_opts.qos = qos;
          LOG(LOG_INFO, "MQTT: Subscribing to '%s'\n", curr->mqtt_topic);
          mg_mqtt_sub(nc, &sub_opts);
        }
      }

      // Any MQTT sensors we need to subscribe to?
      for (acd_key_t *curr = _aquachemd_data->keys; curr != NULL; curr = curr->next) {
        if (curr->type == KEY_TYPE_MQTT_TEMP) {
          memset(&sub_opts, 0, sizeof(sub_opts));
          sub_opts.topic = mg_str(curr->data.mqtt.topic);
          sub_opts.qos = qos;
          LOG(LOG_INFO, "MQTT: Subscribing to '%s'\n", curr->data.mqtt.topic);
          mg_mqtt_sub(nc, &sub_opts);
        }
      }
      
      LOG(LOG_INFO, "MQTT: sending Alive message (last will message)\n");
      snprintf(aq_topic, 24, "%s/%s", _acdconfig_.mqtt_aquachemd_topic,MQTT_LWM_TOPIC);
      send_mqtt(nc, aq_topic ,MQTT_ON);

      publish_mqtt_discovery( _aquachemd_data, nc);
    }
    break;

  case MG_EV_MQTT_CMD:
    //LOG(LOG_NOTICE, "MQTT: MG_EV_MQTT_CMD command, add code / need to replocate MG_EV_MQTT_PUBACK MG_EV_MQTT_SUBACK\n");
    break;
  //case MG_EV_MQTT_PUBLISH:
  case MG_EV_MQTT_MSG:
    bool found=false;
    mqtt_msg = (struct mg_mqtt_message *)ev_data;
    
    // We are only subscribed to aquachemd topic, (so not checking that).
    // Just check we have "set" as string end
    if ( FAST_SUFFIX_3_CI(mqtt_msg->topic.buf, mqtt_msg->topic.len, "set"))
    {
      LOG(LOG_DEBUG, "MQTT: received (msg_id: %d), %.*s\n", mqtt_msg->id, mqtt_msg->topic.len, mqtt_msg->topic.buf);
      action_mqtt_message(nc, mqtt_msg);
      found=true;
    } else {
      for (acd_condition *curr = _aquachemd_data->conditions; curr != NULL; curr = curr->next) {
        if (curr->type == COND_MQTT) {
          if (strncasecmp(mqtt_msg->topic.buf, curr->mqtt_topic, mqtt_msg->topic.len) == 0) {
            LOG(LOG_DEBUG, "MQTT: received (msg_id: %d), %.*s checking\n", mqtt_msg->id, mqtt_msg->topic.len, mqtt_msg->topic.buf);
            action_mqtt_condition_message(curr, mqtt_msg);
            found=true;
          }
        }
      }
      for (acd_key_t *curr = _aquachemd_data->keys; curr != NULL; curr = curr->next) {
        if (curr->type == KEY_TYPE_MQTT_TEMP) {
          if (strncasecmp(mqtt_msg->topic.buf, curr->data.mqtt.topic, mqtt_msg->topic.len) == 0) {
            LOG(LOG_DEBUG, "MQTT: received (msg_id: %d), %.*s checking\n", mqtt_msg->id, mqtt_msg->topic.len, mqtt_msg->topic.buf);
            action_mqtt_sensor_message(curr, mqtt_msg);
            found=true;
          }
        }
      }
    }
    if (!found) {
      LOG(LOG_DEBUG, "MQTT: received (msg_id: %d), %.*s ignoring\n", mqtt_msg->id, mqtt_msg->topic.len, mqtt_msg->topic.buf);
    }
    break;
  }
}



static void ws_send(struct mg_connection *nc, char *msg)
{
  int size = strlen(msg);
  
  mg_ws_send(nc, msg, size, WEBSOCKET_OP_TEXT);
  
  //LOG(LOG_DEBUG, "WS: Sent %d characters '%s'\n",size, msg);
}

void send_mqtt_int_msg(struct mg_connection *nc, char *dev_name, int value) {
  static char mqtt_pub_topic[250];
  static char msg[11];

  sprintf(msg, "%d", value);
  sprintf(mqtt_pub_topic, "%s/%s", _acdconfig_.mqtt_aquachemd_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

void send_mqtt_float_msg(struct mg_connection *nc, char *dev_name, float value) {
  static char mqtt_pub_topic[250];
  static char msg[11];

  sprintf(msg, "%.2f", value);
  sprintf(mqtt_pub_topic, "%s/%s", _acdconfig_.mqtt_aquachemd_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

/*
// replaced with send_mqtt_acd_state_msg()
void send_mqtt_key_status_msg(struct mg_connection *nc, acd_key_t *key) {
  static char mqtt_pub_topic[250];
  static char msg[4];

  sprintf(mqtt_pub_topic, "%s/%s/status", _acdconfig_.mqtt_aquachemd_topic, key->ID);
  sprintf(msg, "%d", key->state);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
*/

void send_mqtt_acd_state_msg(struct mg_connection *nc, char *ID, acd_state_t state) {
  static char mqtt_pub_topic[250];
  static char msg[4];

  sprintf(mqtt_pub_topic, "%s/%s/status", _acdconfig_.mqtt_aquachemd_topic, ID);
  sprintf(msg, "%d", state);
  send_mqtt(nc, mqtt_pub_topic, msg);
}

/*
void send_mqtt_string_msg(struct mg_connection *nc, const char *dev_name, const char *msg) {
  static char mqtt_pub_topic[250];

  sprintf(mqtt_pub_topic, "%s/%s", _aqconfig_.mqtt_aq_topic, dev_name);
  send_mqtt(nc, mqtt_pub_topic, msg);
}
*/

void send_mqtt_temp_msg(struct mg_connection *nc, char *dev_name, float value)
{
  // Incase we need to do degc to f conversion, we can do it here.
  send_mqtt_float_msg(nc, dev_name, (float)value);
}

void mqtt_broadcast_aquachemdstate(struct mg_connection *nc)
{

  char topic[250];
  char msg[11];

  // Post main ACD state
  send_mqtt_acd_state_msg(nc, _aquachemd_data->keys->ID, _aquachemd_data->keys->state);

  // Post conditions if enabled.  
  if (_acdconfig_.post_condition == true) {
    for (acd_condition *curr = _aquachemd_data->conditions; curr != NULL; curr = curr->next) {
      send_mqtt_acd_state_msg(nc, curr->ID, curr->met?ACD_LED_ON:ACD_LED_OFF);
    }
  }

  // Post sensors
  for (acd_key_t *curr = _aquachemd_data->keys->next; curr != NULL; curr = curr->next) { 
    //send_mqtt_key_status_msg(nc, curr);
    send_mqtt_acd_state_msg(nc, curr->ID, curr->state);
    if (curr->state == ACD_LED_ON)
      send_mqtt_float_msg(nc, curr->ID, curr->value);
  }

  // Post to aqualinkd if enabled (only pH & ORP)
  if (_acdconfig_.mqtt_aqualinkd_topic != NULL) {
    for (acd_key_t *curr = _aquachemd_data->keys->next; curr != NULL; curr = curr->next) {
      if (curr->type == KEY_TYPE_EZO_PH && curr->index == MASTER_ID && curr->state == ACD_LED_ON) {
        sprintf(msg, "%.2f", curr->value);
        sprintf(topic, "%s/CHEM/pH/set", _acdconfig_.mqtt_aqualinkd_topic);
        send_mqtt(nc, topic, msg);
        LOG(LOG_DEBUG, "MQTT: Broadcasted pump pH %s to Aqualinkd\n", msg);
      } else if (curr->type == KEY_TYPE_EZO_ORP && curr->index == MASTER_ID && curr->state == ACD_LED_ON) {
        sprintf(msg, "%.2f", curr->value);
        sprintf(topic, "%s/CHEM/ORP/set", _acdconfig_.mqtt_aqualinkd_topic);
        send_mqtt(nc, topic, msg);
        LOG(LOG_DEBUG, "MQTT: Broadcasted pump ORP %s to Aqualinkd\n", msg);
      }
    }
/*
    sprintf(msg, "%.2f", _aquachemd_data->ph_reading.value);
    sprintf(mqtt_pub_topic, "%s/CHEM/pH/set", _acdconfig_.mqtt_aqualinkd_topic);
    send_mqtt(nc, mqtt_pub_topic, msg);

    sprintf(msg, "%.2f", _aquachemd_data->orp_reading.value);
    sprintf(mqtt_pub_topic, "%s/CHEM/ORP/set", _acdconfig_.mqtt_aqualinkd_topic);
    send_mqtt(nc, mqtt_pub_topic, msg);
*/    
    //LOG(LOG_DEBUG, "MQTT: Broadcasted pH %.2f and ORP %.2f to Aqualinkd\n", _aquachemd_data->ph_reading.value, _aquachemd_data->orp_reading.value);
  }
}


void reset_last_mqtt_status()
{
}

void broadcast_aquachemdstate(struct mg_connection *nc) 
{
  struct mg_connection *c;
  static int mqtt_count=0;

  LOG(LOG_NOTICE, "Broadcasting Aquachemd state to websockets and MQTT\n");

  // Reconnect MQTT if needed.
  if (_mqtt_exit_flag == true) {
    mqtt_count++;
    //if (mqtt_count >= 10) {
    if (mqtt_count >= 1) { // changed to 1 when cleaned up all the MQTT spaming.  may want to delete count completely in the future
      start_mqtt(nc->mgr);
      mqtt_count = 0;
    }
  }

  for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
    //if (is_websocket(c) && !is_websocket_simulator(c)) // No need to broadcast status messages to simulator.
    if (is_websocket(c)) {
      LOG(LOG_DEBUG, "ws_send not implimented");
      //ws_send(c, data); // Data should be JSON string 
    } else if (is_mqtt(c)) {
      mqtt_broadcast_aquachemdstate(c);
    }
  }

  CLEAR_DIRTY(_aquachemd_data->is_dirty);
}





void start_mqtt(struct mg_mgr *mgr) {
  
  //generate_mqtt_id(_acdconfig_.mqtt_ID, MQTT_ID_LEN);

  //LOG(LOG_WARNING, "NOT Starting MQTT client, need to check code\n");
  if ( _acdconfig_.mqtt_server == NULL || _acdconfig_.mqtt_aquachemd_topic == NULL ) 
    return;

  char aq_topic[30];
  char *mqtt_ID = generate_mqtt_id();
  LOG(LOG_NOTICE, "Starting MQTT client to %s, id %s\n", _acdconfig_.mqtt_server, mqtt_ID);

  snprintf(aq_topic, 24, "%s/%s", _acdconfig_.mqtt_aquachemd_topic,MQTT_LWM_TOPIC);

  struct mg_mqtt_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.user = mg_str(_acdconfig_.mqtt_user);
    opts.pass = mg_str(_acdconfig_.mqtt_passwd);
    //opts.client_id = mg_str(_acdconfig_.mqtt_ID);
    opts.client_id = mg_str(mqtt_ID);

    //opts.keepalive = 5; // This seems to kill connection for some reason, and not sent heartbeat
    opts.clean = true;
    //opts.version = 4; // Maybe 5
    opts.message = mg_str(MQTT_OFF); // will_message
    opts.topic = mg_str(aq_topic); // will_topic
    

  struct mg_connection *nc = mg_mqtt_connect(mgr, _acdconfig_.mqtt_server, &opts, ev_handler, NULL);
  if ( nc == NULL ) {
    LOG(LOG_ERR, "Failed to create MQTT listener to %s\n", _acdconfig_.mqtt_server);
  } else {
    set_mqttconnecting(nc);
    reset_last_mqtt_status();
    _mqtt_exit_flag = false; // set here to stop multiple connects, if it fails truley fails it will get set to false.
  }

}




bool network_service(struct mg_mgr *mgr, struct aquachemdata *acdata) {
  struct mg_connection *nc;
  _aquachemd_data = acdata;

  signal(SIGTERM, net_signal_handler);
  signal(SIGINT, net_signal_handler);
  //signal(SIGRESTART, net_signal_handler);
  setvbuf(stdout, NULL, _IOLBF, 0);
  setvbuf(stderr, NULL, _IOLBF, 0);
  
  mg_log_set(_acdconfig_.mg_log_level);
  mg_log_set_fn(mg_logger, NULL);

  const char *nameserver = get_ip_address_of_nameserver();
  mg_mgr_init(mgr);

  if (nameserver != NULL)
    mgr->dns4.url = nameserver;

  LOG(LOG_NOTICE, "Starting web server on %s\n", _acdconfig_.listen_address);
  //nc = mg_bind(mgr, _acdconfig_.listen_address, ev_handler);
  nc = mg_http_listen(mgr, _acdconfig_.listen_address, ev_handler, mgr);
  if (nc == NULL) {
    LOG(LOG_ERR, "Failed to create listener on port %s\n",_acdconfig_.listen_address);
    return false;
  }

  // Set default web options
  _http_server_opts.root_dir = _acdconfig_.web_directory;
  _http_server_opts.extra_headers = CACHE; 
  _http_server_opts.ssi_pattern = NULL;

  _http_server_opts_nocache.root_dir = _acdconfig_.web_directory;
  _http_server_opts_nocache.extra_headers = NO_CACHE;
  _http_server_opts_nocache.ssi_pattern = NULL;
  // Start MQTT
  start_mqtt(mgr);



  return true;
}

void *net_services_thread( void *ptr )
{
  _aquachemd_data = (struct aquachemdata *) ptr;

  if (!network_service(&_mgr, _aquachemd_data)) {
    //LOG(LOG_ERR, "Failed to start network services\n");
    // Not the best way to do this (have thread exit process), but forks for the moment.
    _keepNetServicesRunning = false;
    LOG(LOG_ERR, "Can not start webserver on port %s.\n", _acdconfig_.listen_address);
    exit(EXIT_FAILURE);
    goto f_end;
  }

  while (_keepNetServicesRunning == true)
  {
    mg_mgr_poll(&_mgr, 100);

    if (_aquachemd_data->is_dirty == true /*|| _broadcast == true*/) {
      broadcast_aquachemdstate(_mgr.conns);
      CLEAR_DIRTY(_aquachemd_data->is_dirty);
    } else {
      //LOG(LOG_DEBUG, "No state change, not broadcasting\n");
    }
  }

f_end:
  LOG(LOG_NOTICE, "Stopping network services thread\n");
  mg_mgr_free(&_mgr);

  pthread_exit(0);

}


bool start_net_services(struct aquachemdata *acddata) 
{
  // Not the best way to see if we are running, but works for now.
  if (_net_thread_id != 0 && _keepNetServicesRunning) {
    LOG(LOG_NOTICE, "Network services thread is already running, not starting\n");
    return true;
  }

  _keepNetServicesRunning = true;

  LOG(LOG_NOTICE, "Starting network services thread\n");

  if( pthread_create( &_net_thread_id , NULL ,  net_services_thread, (void*)acddata) < 0) {
    LOG(LOG_ERR, "could not create network thread\n");
    return false;
  }

  pthread_detach(_net_thread_id);

  return true;
}
/*
 * Copyright (c) 2014-2018 Cesanta Software Limited
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Cloud Messaging support.
 * https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-devguide-messaging
 */

#include "mgos_azure.h"
#include "mgos_azure_internal.h"

#include "common/cs_dbg.h"

#include "frozen.h"
#ifdef MGOS_HAVE_MJS
#include "mjs.h"
#endif

#include "mgos_mqtt.h"

static void mgos_azure_cm_ev(struct mg_connection *nc, const char *topic,
                             int topic_len, const char *msg, int msg_len,
                             void *ud) {
  struct mg_str ps = MG_MK_STR("/devicebound/%24");
  struct mg_str ts = {.p = topic, .len = topic_len};
  struct mgos_azure_c2d_arg c2da = {
      .body = {.p = msg, .len = msg_len},
  };
  const char *pstart;
  if ((pstart = mg_strstr(ts, ps)) != NULL) {
    c2da.props.p = pstart + ps.len;
    c2da.props.len = (ts.p + ts.len) - c2da.props.p;
  }
  LOG(LL_DEBUG, ("Cloud msg: '%.*s' '%.*s'", (int) c2da.body.len, c2da.body.p,
                 (int) c2da.props.len, c2da.props.p));
  mgos_event_trigger(MGOS_AZURE_EV_C2D, &c2da);
  (void) nc;
  (void) ud;
}

bool mgos_azure_send_d2c_msg(const struct mg_str props,
                             const struct mg_str body) {
  char *topic;
  bool res = false;
  struct mg_str did = mgos_azure_get_device_id();
  if (did.len == 0) goto out;
  mg_asprintf(&topic, 0, "devices/%.*s/messages/events/%s%.*s", (int) did.len,
              did.p, (props.len > 0 ? "%24" : ""), (int) props.len, props.p);
  if (topic != NULL) {
    res = mgos_mqtt_pub(topic, body.p, body.len, 0 /* qos */, 0 /* retain */);
    free(topic);
  }
out:
  return res;
}

bool mgos_azure_send_d2c_msgf(const struct mg_str props, const char *json_fmt,
                              ...) {
  va_list ap;
  va_start(ap, json_fmt);
  char *body = json_vasprintf(json_fmt, ap);
  va_end(ap);
  return mgos_azure_send_d2c_msg(props, mg_mk_str(body));
}

bool mgos_azure_send_d2c_msgp(const struct mg_str *props,
                              const struct mg_str *body) {
  const struct mg_str ns = MG_NULL_STR;
  return mgos_azure_send_d2c_msg((props ? *props : ns), (body ? *body : ns));
}

bool mgos_azure_cm_init(void) {
  if (!mgos_sys_config_get_azure_enable_cm()) return true;
  struct mg_str did = mgos_azure_get_device_id();
  char *topic = NULL;
  mg_asprintf(&topic, 0, "devices/%.*s/messages/devicebound/#", (int) did.len,
              did.p);
  mgos_mqtt_sub(topic, mgos_azure_cm_ev, NULL);
  free(topic);
  return true;
}

/* FFI helpers */
#ifdef MGOS_HAVE_MJS
static const struct mjs_c_struct_member c2d_def[] = {
    {"body", offsetof(struct mgos_azure_c2d_arg, body),
     MJS_FFI_CTYPE_STRUCT_MG_STR},
    {"props", offsetof(struct mgos_azure_c2d_arg, props),
     MJS_FFI_CTYPE_STRUCT_MG_STR},
    {NULL, 0, MJS_FFI_CTYPE_NONE},
};

const struct mjs_c_struct_member *mgos_azure_get_c2dd(void) {
  return c2d_def;
}
#endif /* HAVE_MJS */

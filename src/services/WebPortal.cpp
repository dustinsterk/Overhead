#include "WebPortal.h"
#include "Settings.h"
#include <AsyncJson.h>
#include <ElegantOTA.h>
#include <ESPmDNS.h>

static const char kIndexHtml[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Overhead</title>
<style>body{font:15px system-ui;margin:1.2rem;max-width:40rem;background:#0b1018;color:#e6e8f0}
a{color:#50aaff}code{background:#1a2030;padding:.1rem .3rem;border-radius:3px}
pre{background:#11161f;padding:.6rem;border-radius:6px;overflow:auto}</style></head>
<body><h2>Overhead</h2>
<p><a href=/update>Firmware update (OTA)</a></p>
<h3>Status</h3><pre id=s>loading…</pre>
<h3>Settings</h3><pre id=cfg>loading…</pre>
<p>Edit settings via <code>POST /api/settings</code> with a JSON body of the keys to change.</p>
<script>
const j=(u,o)=>fetch(u,o).then(r=>r.json());
j('/api/status').then(d=>s.textContent=JSON.stringify(d,null,2));
j('/api/settings').then(d=>cfg.textContent=JSON.stringify(d,null,2));
</script></body></html>
)HTML";

bool WebPortal::begin(Settings* s, const String& hostname) {
  _s = s;

  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.printf("[web] http://%s.local/\n", hostname.c_str());
  }

  _server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", kIndexHtml);
  });

  _server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    JsonDocument doc;
    if (_statusFn) _statusFn(doc);
    String out; serializeJson(doc, out);
    req->send(200, "application/json", out);
  });

  _server.on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* req) {
    String out; serializeJson(_s->doc(), out);
    req->send(200, "application/json", out);
  });

  // POST/PUT JSON body -> merge changed keys into settings and persist.
  auto* setHandler = new AsyncCallbackJsonWebHandler("/api/settings",
      [this](AsyncWebServerRequest* req, JsonVariant& json) {
        if (!json.is<JsonObject>()) { req->send(400, "application/json", "{\"ok\":false}"); return; }
        for (JsonPair kv : json.as<JsonObject>()) _s->doc()[kv.key()] = kv.value();
        _s->save();
        req->send(200, "application/json", "{\"ok\":true}");
      });
  _server.addHandler(setHandler);

  // OTA + settings basic-auth (spec §13).
  ElegantOTA.setAuth(_s->getString("otaUser", "admin").c_str(),
                     _s->getString("otaPass", "overhead").c_str());
  ElegantOTA.begin(&_server);

  _server.begin();
  return true;
}

void WebPortal::loop() { ElegantOTA.loop(); }

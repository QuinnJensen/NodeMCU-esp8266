// metrics_server.cpp - shared Prometheus endpoint
#include "metrics_server.h"
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "app_state.h"
#include "app_config.h"
#include "util.h"

static const char* sPrefix = "node";
static MetricsExtraFn sExtra = nullptr;

void setMetricsNamePrefix(const char* prefix) { if (prefix && prefix[0]) sPrefix = prefix; }
void setMetricsExtra(MetricsExtraFn fn) { sExtra = fn; }

String prometheusEscaped(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (uint16_t i = 0; i < in.length(); i++) {
    char c = in.charAt(i);
    if (c == '\\' || c == '"') out += '\\';
    if (c == '\n' || c == '\r') out += ' ';
    else out += c;
  }
  return out;
}

String buildPrometheusMetrics() {
  String idLabel  = prometheusEscaped(safeDeviceId());
  String verLabel = prometheusEscaped(String(buildVersion));
  String pfx = String(sPrefix);
  metricsScrapeCount++;
  String m;
  m.reserve(4096);
  m += "# HELP " + pfx + "_build_info Firmware build information.\n";
  m += "# TYPE " + pfx + "_build_info gauge\n";
  m += pfx + "_build_info{id=\"" + idLabel + "\",version=\"" + verLabel + "\"} 1\n";
  m += "# HELP " + pfx + "_node_online Node online state.\n";
  m += "# TYPE " + pfx + "_node_online gauge\n";
  m += pfx + "_node_online{id=\"" + idLabel + "\"} 1\n";
  m += "# HELP " + pfx + "_wifi_connected WiFi connected state.\n";
  m += "# TYPE " + pfx + "_wifi_connected gauge\n";
  m += pfx + "_wifi_connected{id=\"" + idLabel + "\"} ";
  m += (WiFi.status() == WL_CONNECTED ? "1\n" : "0\n");
  m += "# HELP " + pfx + "_mqtt_connected MQTT connected state.\n";
  m += "# TYPE " + pfx + "_mqtt_connected gauge\n";
  m += pfx + "_mqtt_connected{id=\"" + idLabel + "\"} ";
  m += (mqtt.connected() ? "1\n" : "0\n");
  m += "# HELP " + pfx + "_wifi_rssi_dbm WiFi RSSI in dBm.\n";
  m += "# TYPE " + pfx + "_wifi_rssi_dbm gauge\n";
  m += pfx + "_wifi_rssi_dbm{id=\"" + idLabel + "\"} " + String(WiFi.RSSI()) + "\n";
  m += "# HELP " + pfx + "_free_heap_bytes Free heap bytes.\n";
  m += "# TYPE " + pfx + "_free_heap_bytes gauge\n";
  m += pfx + "_free_heap_bytes{id=\"" + idLabel + "\"} " + String(ESP.getFreeHeap()) + "\n";
  m += "# HELP " + pfx + "_uptime_seconds Uptime in seconds.\n";
  m += "# TYPE " + pfx + "_uptime_seconds counter\n";
  m += pfx + "_uptime_seconds{id=\"" + idLabel + "\"} " + String((millis() - bootMillis) / 1000UL) + "\n";
  m += "# HELP " + pfx + "_prometheus_port Prometheus listener port.\n";
  m += "# TYPE " + pfx + "_prometheus_port gauge\n";
  m += pfx + "_prometheus_port{id=\"" + idLabel + "\"} " + String(config.prometheusPort) + "\n";
  m += "# HELP " + pfx + "_mqtt_publish_total Total successful MQTT publishes.\n";
  m += "# TYPE " + pfx + "_mqtt_publish_total counter\n";
  m += pfx + "_mqtt_publish_total{id=\"" + idLabel + "\"} " + String(mqttPublishCount) + "\n";
  m += "# HELP " + pfx + "_prometheus_scrape_total Total Prometheus scrapes served.\n";
  m += "# TYPE " + pfx + "_prometheus_scrape_total counter\n";
  m += pfx + "_prometheus_scrape_total{id=\"" + idLabel + "\"} " + String(metricsScrapeCount) + "\n";

  if (sExtra) sExtra(m);

  return m;
}

void startMetricsServer() {
  if (metricsServer) {
    delete metricsServer;
    metricsServer = nullptr;
  }
  metricsServer = new ESP8266WebServer(config.prometheusPort);
  metricsServer->on("/metrics", HTTP_GET, []() {
    metricsServer->send(200, "text/plain; charset=utf-8", buildPrometheusMetrics());
  });
  metricsServer->on("/", HTTP_GET, []() {
    metricsServer->send(200, "text/plain", "Prometheus metrics available at /metrics");
  });
  metricsServer->begin();
}

void serviceMetricsServer() {
  if (metricsServer) metricsServer->handleClient();
}

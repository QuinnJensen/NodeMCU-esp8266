// metrics_server.h - shared Prometheus metrics endpoint
#pragma once
#include <Arduino.h>

// Sketch-supplied extra-metrics callback. Called during /metrics handling
// after the shared common metrics have been emitted. Append additional
// Prometheus text using the supplied String reference.
typedef void (*MetricsExtraFn)(String& out);

// Per-sketch label prefix for the common metric names (e.g. "temp", "wh", "uhf").
// Default is "node".
void setMetricsNamePrefix(const char* prefix);
void setMetricsExtra(MetricsExtraFn fn);

void startMetricsServer();
void serviceMetricsServer();

String prometheusEscaped(const String& in);
String buildPrometheusMetrics();

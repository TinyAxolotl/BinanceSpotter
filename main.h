#pragma once;

typedef struct {
  char *ssid;
  char *password;
} wifi_config;

typedef struct {
  char **coin_list;
  uint8_t num_of_coins;
  uint8_t update_interval_s;
} binance_config;

typedef struct {
  uint8_t brightness;
  char *theme;
  uint8_t coin_switch_interval_s;
} display_config;

typedef struct {
  wifi_config wifi;
  binance_config binance;
  display_config display;
} spotter_config;

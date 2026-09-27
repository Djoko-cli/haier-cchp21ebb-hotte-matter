// ===========================================================================
//  Fronts de la ligne D (spec 8.4) : interruption GPIO sur les deux fronts de
//  la broche d'ecoute, par le service ISR d'IDF (pas d'attachInterrupt
//  d'Arduino). Elle tient l'heure du dernier front (silence avant une
//  injection) et le nombre de fronts ; elle coexiste avec le canal RMT sur la
//  meme broche. Le niveau du bus se lit sur la broche, remis dans le sens du
//  bus selon l'etage (captureReglages().inverse).
// ===========================================================================
#include "bord.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_timer.h>

#include "capture_rmt.h"
#include "config.h"

namespace {

portMUX_TYPE sMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t sPin = kPinEcoute;
uint64_t sDernierUs = 0;  // sous sMux : deux mots de 32 bits
volatile uint32_t sFronts = 0;

void IRAM_ATTR surFront(void *) {
  const uint64_t t = esp_timer_get_time();
  portENTER_CRITICAL_ISR(&sMux);
  sDernierUs = t;
  sFronts = sFronts + 1;
  portEXIT_CRITICAL_ISR(&sMux);
}

}  // namespace

bool bordBegin(uint8_t pin) {
  gpio_config_t c = {};
  c.pin_bit_mask = 1ULL << pin;
  c.mode = GPIO_MODE_INPUT;
  c.pull_up_en = GPIO_PULLUP_ENABLE;  // comme le pilote RMT : sans effet avec le 10 k de collecteur
  c.pull_down_en = GPIO_PULLDOWN_DISABLE;
  c.intr_type = GPIO_INTR_ANYEDGE;
  esp_err_t err = gpio_config(&c);
  if (err == ESP_OK) {
    err = gpio_install_isr_service(0);
    if (err == ESP_ERR_INVALID_STATE) err = ESP_OK;  // deja installe (Arduino ou un autre module)
  }
  if (err == ESP_OK) err = gpio_isr_handler_add((gpio_num_t)pin, surFront, nullptr);
  if (err != ESP_OK) {
    log_e("[bord] GPIO%u : %s", (unsigned)pin, esp_err_to_name(err));
    return false;
  }
  sPin = pin;
  return true;
}

uint64_t bordDernierUs() {
  portENTER_CRITICAL(&sMux);
  const uint64_t t = sDernierUs;
  portEXIT_CRITICAL(&sMux);
  return t;
}

bool bordBusHaut() {
  const bool gpioHaut = gpio_get_level((gpio_num_t)sPin) != 0;
  return captureReglages().inverse ? !gpioHaut : gpioHaut;
}

uint32_t bordFronts() { return sFronts; }

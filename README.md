# es-iot-wtank

Estudio Sustenta IoT to see water tank level in real time using HC-SR04

## Set up

### Before compiling in arduino IDE:

1. Update [libraries](https://github.com/roy-mdr/es-iot-libs)
1. Change the client id in line 10
1. Set the correct server at line 51 and 77
1. You could change the pins in line 67, 68 and 73

### On server:

1. Configure devices and controllers databases

## To Do

- [ ] Add HTTPS
- [ ] OTA updates para firmware (diseñar para device targeted)
- [ ] ESP-NOW para crear mesh de dispositivos y sólo un cerebro conectado al broker
- [ ] ping pong support on every device
- [ ] Migrate to `async-timer` library [(to-do)](https://github.com/roy-mdr/es-iot-libs#to-dos)
- [ ] Update status on threshold change (ej. each 10cm)
- [ ] Set server to config. limits (min = 0%, max = 100%)
- [ ] Responf with % not distance
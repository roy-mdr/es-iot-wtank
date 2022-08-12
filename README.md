# es-iot-wtank

Estudio Sustenta IoT to see water tank level in real time using HC-SR04

## Set up

### Before compiling in arduino IDE:

1. Update [libraries](https://github.com/roy-mdr/es-iot-libs)
1. Change the client id in line 10
1. Set the correct server at lines 51 and 86
1. You can change the pins in lines 67, 68, and 82
1. You can change the dince threshold and interval between measurement in lines 73 and 79

### On server:

1. Configure devices and controllers databases

## To Do

- [ ] Add HTTPS
- [ ] OTA updates para firmware (diseñar para device targeted)
- [ ] ESP-NOW para crear mesh de dispositivos y sólo un cerebro conectado al broker
- [ ] ping pong support on every device
- [ ] Migrate to `async-timer` library [(to-do)](https://github.com/roy-mdr/es-iot-libs#to-dos)
- [x] Update status on threshold change (ej. each 10cm)
- [ ] Set server to config. limits (min = 0%, max = 100%, soft_min: (start pump), soft_max: (stop_pump))
- [ ] Responf with % not distance
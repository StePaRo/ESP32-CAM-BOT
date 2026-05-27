# ESP32-CAM-BOT
ESP32-CAM-BOT-MODULE
A lightweight camera robot sketch for the GOOUUU / Freenove ESP32-S3-CAM module with tank-style driving controlled via a mobile-friendly web interface.
Features

MJPEG live stream on port 81
Joystick control in the browser (touch & mouse), tank mixing for differential drive
Headlight switchable via web interface
Live calibration of minimum PWM, maximum speed and individual motor correction factors — adjustable directly in the browser without reflashing

Hardware

GOOUUU / Freenove ESP32-S3-CAM
Motor driver: MX1508
2 DC motors (tank/differential drive)
Optional: white LED as headlight

Pin Assignment
FunctionGPIOMotor Left A/B1, 2Motor Right A/B19, 20LED Headlight21
Access
Connect to the same WiFi network as the ESP32, then open http://<IP-address> in your browser. The IP address is printed to the Serial Monitor on startup.
Notes

Camera pins GPIO 4–13 and 15–18 are reserved by the ESP32-S3-CAM hardware and cannot be used for other functions
The MX1508 motor driver is directly compatible with the ESP32's 3.3V PWM signals — no level shifter needed

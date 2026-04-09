# parksmart-esp32
ESP32 based smart parking system with web dashboard ,OTP based booking system and other simple features.

# ParkSmart-Smart Parking System (ESP32 + Web Dashboard)

## 📌 Overview

This project is a smart parking system built using an ESP32 that monitors parking slots in real-time, allows users to book slots through a web interface, and controls gate access using a password-based system.

It integrates hardware (sensors + servo motor) with a web dashboard and SMS notifications to simulate a real-world automated parking solution.

---

## ⚙️ Features

* Real-time parking slot detection using sensors
* Online slot booking via web dashboard
* Automatic password generation for booked slots
* Gate access using password authentication
* SMS notification for booking confirmation
* Auto-expiry of booking when vehicle leaves
* Parking time and cost calculation
* LED indicators for slot status

---

## 🧠 Tech Stack

* **ESP32 (Arduino framework)**
* **HTML(Frontend)**
* **Fast2SMS API (SMS service)**
* **IR Sensors + Servo Motor + LEDs**

---

## 🏗️ System Architecture

* Sensors detect slot occupancy → ESP32 processes data
* ESP32 hosts a web server → dashboard fetches live data
* User books slot → password generated
* Password used at gate → servo opens gate
* SMS sent via API with booking details

---

## 🖥️ Web Dashboard

* View available and occupied slots
* Book or free slots
* Enter password to open gate
* See parking time and cost

---

## 🔌 Hardware Components

* ESP32
* IR Sensors (for slot detection and gate)
* Servo Motor (gate control)
* LEDs (slot indication)
* External power supply (recommended for stability)

---

## 🚀 Setup Instructions

### 1. ESP32 Setup

* Open `esp32/parking_system.ino`
* Replace:

  * WiFi credentials
  * Fast2SMS API key
  * Phone number
* Upload code to ESP32

---

### 2. Web Dashboard

* Open `web/index.html`
* Update:

  * ESP32 IP address

---

### 3. Run the System

* Connect ESP32 and your device to the same WiFi network
* Open the HTML file in your browser
* Start booking and testing

---

## ⚠️ Notes

* Servo motor should use a separate power supply for stability
* SMS API requires minimum recharge to enable usage
* System is designed for demonstration purposes

---

## 📈 Future Improvements

* User authentication system
* Mobile app integration
* Payment system for parking
* Slot navigation guidance
* Cloud database integration

---

## 👨‍💻 Author

Krishna Singh


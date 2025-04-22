<p align="left">
    <a href="https://github.com/Scout064/esp32-dht22-monitoring/releases/tag/v1.1">
        <img src="https://img.shields.io/badge/Latest_Stable_Release-v1.1-brightgreen" /></a>
    <a href="https://github.com/Scout064/esp32-dht22-monitoring/releases/tag/v1.1.5-alpha">
    <img src="https://img.shields.io/badge/Latest_Release-v1.1.5--alpha-red" /></a>
    <a href="">
        <img src="https://img.shields.io/badge/Arduino_IDE_Compile-passed-brightgreen" /></a>
    <a href="">
        <img src="https://img.shields.io/badge/Tested_on_ESP32-passed-brightgreen" /></a>
    <a href="https://github.com/Scout064/esp32-dht22-monitoring/issues">
        <img src="https://img.shields.io/badge/Known_Issues-0-brightgreen" /></a>
    <a href="#further-development">
        <img src="https://img.shields.io/badge/Development_Queue-0-blue" /></a>
</p>

# ESP32 DHT22 Sensor API Server

Firmware for ESP32 that exposes a web server and REST API to access DHT22 sensor readings (temperature, humidity), configure WiFi and interval settings, control device reboot or reset, and perform secure web OTA updates.

## 🔧 Features
- Read temperature and humidity from DHT22
- Async web server with fast response
- Configurable via REST API (WiFi, polling rate, admin key)
- Swagger UI at `/docs` for interactive API documentation
- Prometheus metrics at `/metrics` for Grafana
- CORS-enabled for frontend apps
- Web OTA updates via `/update` (admin protected)
- MD5 checksum validation for OTA uploads (Not Optional)
- Tracks and exposes last OTA error + both MD5s via `/api/debug`
- Upload `.txt` file first, then `.bin` firmware for integrity check
- Admin key protected endpoints for sensitive operations

## 🌐 Endpoints

### Sensor Data
| Method | Path             | Auth | Description                      |
|--------|------------------|------|----------------------------------|
| GET    | `/temperature`   | ❌   | Returns current temperature (°C) |
| GET    | `/humidity`      | ❌   | Returns current humidity (%)     |

### Configuration
| Method | Path                    | Auth | Description                                |
|--------|-------------------------|------|--------------------------------------------|
| POST   | `/api/update-wifi`      | ✅   | Change WiFi credentials                    |
| POST   | `/api/update-interval`  | ✅   | Update sensor polling interval (≥500ms)    |
| POST   | `/api/update-key`       | ✅   | Change admin key securely                  |

### Device Management
| Method | Path                | Auth | Description                        |
|--------|---------------------|------|------------------------------------|
| POST   | `/api/reboot`       | ✅   | Reboots the ESP32 device           |
| POST   | `/api/factory-reset`| ✅   | Clears all preferences and restarts |
| GET    | `/update`           | ✅   | Web-based OTA HTML upload form     |
| POST   | `/update`           | ✅   | Upload `md5` (.txt) or `update` (.bin)     |

### Logging & Monitoring
| Method | Path                 | Auth | Description                                  |
|--------|----------------------|------|----------------------------------------------|
| GET    | `/api/logs`          | ✅   | Returns last 10 log events                   |
| GET    | `/metrics`           | ❌   | Prometheus metrics format for scrape         |
| GET    | `/api/status`        | ❌   | JSON with temp, humidity, uptime, IP, SSID, FW Version, Buildtime, OTA Error |
| GET    | `/api/debug`         | ✅   | Returns last OTA error, expected & actual MD5 |
| POST   | `/api/reset-ota-log` | ✅   | Clears last recorded OTA error message       |

### API Docs (Swagger)
| Method | Path     | Auth | Description               |
|--------|----------|------|---------------------------|
| GET    | `/docs`  | ❌   | Swagger UI API explorer   |

## 🔐 Authentication

> **Note:** For protected endpoints, use `?key=...` query param.
- Default admin key: `secret123`

**Example:**
```bash
curl -X POST http://192.168.1.200/api/reboot?key=secret123
```

## 💾 Preferences Structure
| Namespace | Key         | Purpose                         |
|-----------|-------------|---------------------------------|
| `wifi`    | `ssid`      | Stored WiFi SSID                |
| `wifi`    | `password`  | Stored WiFi Password            |
| `config`  | `interval`  | Sensor polling interval (ms)    |
| `auth`    | `adminKey`  | Admin key for auth-protected routes |

## 📌 Requirements
- ESP32 (DevKit or equivalent)
- DHT22 sensor (wired to GPIO5)
- Arduino IDE or PlatformIO
- WiFi access

## :electric_plug: Connection Diagram
![connection](https://github.com/user-attachments/assets/c09dd7ac-c429-49e9-b580-26d1cb084c49)

## 🚀 Getting Started
1. Connect DHT22 sensor to GPIO 5.
2. Flash firmware to ESP32.
3. Monitor Serial for IP (115200 baud).
4. Access your ESP32 via browser: `http://<ESP32-IP>/docs`

## 📊 Prometheus Integration
Add this to your `prometheus.yml` scrape config:
```yaml
scrape_configs:
  - job_name: 'esp32-dht'
    metrics_path: /metrics
    static_configs:
      - targets: ['<ESP32-IP>:80']
```
> Then import metrics like `esp32_temperature`, `esp32_humidity`, `esp32_uptime_seconds` in Grafana  

## 🔁 Web OTA Update Instructions
1. Navigate to: `http://<ESP32-IP>/update?key=secret123`
2. **Upload `.txt` file** first containing MD5 checksum of `.bin`
3. **Then upload** compiled `.bin` file from Arduino IDE/PlatformIO
4. Wait for confirmation — if MD5 matches, ESP32 reboots automatically

**MD5 Upload Example:**
```bash
curl -F 'md5=@firmware.md5.txt' http://<ESP32-IP>/update?key=secret123
```

**Firmware Upload Example:**
```bash
curl -F 'update=@firmware_v1.1.1-alpha.bin' http://<ESP32-IP>/update?key=secret123
```

## 📂 File Overview
- `esp32-c6-hdt22-monitor.ino`: main firmware logic
- `OpenAPI.json`: file to import into Postman or Swagger UI

---

Built with ❤️ using [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)

> For issues or contributions, feel free to ([open a GitHub issue](https://github.com/Scout064/esp32-dht22-monitoring))

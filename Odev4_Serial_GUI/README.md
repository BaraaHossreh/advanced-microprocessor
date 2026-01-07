# 🎛️ TM4C123G & C# PC Kontrol Paneli Projesi

Bu proje, **Texas Instruments Tiva C (TM4C123GH6PM)** mikrodenetleyicisi ile **C# (Windows Forms)** tabanlı bir masaüstü yazılımının seri port (UART) üzerinden haberleşmesini sağlayan tam kapsamlı bir gömülü sistem uygulamasıdır.

Proje; donanım seviyesinde sensör okuma ve zamanlama işlemlerini yürütürken, PC arayüzü üzerinden sistemin uzaktan kontrol edilmesini ve izlenmesini sağlar.

![PC Arayüzü]

---

## 🌟 Proje Özellikleri

### 1. Gömülü Sistem (Tiva C) Tarafı
* **🕒 Dijital Saat:** Donanımsal Timer (Timer0) kesmesi ile çalışan hassas saat (SS:DD:sn).
* **🎛️ Analog Okuma (ADC):** PE3 pinine bağlı potansiyometre veya sensör verisinin (0-4095) okunması.
* **📟 LCD Ekran:** 2x16 LCD üzerinde saat, ADC değeri ve PC'den gelen mesajların gösterimi.
* **🕹️ Akıllı Buton Okuma:** "Latching" algoritması ile en hızlı buton basışlarının bile yakalanması.
* **📡 Veri Paketi:** Her saniye PC'ye durum raporu gönderimi.

### 2. PC Arayüzü (C# Windows Forms) Tarafı
* **Bağlantı Yönetimi:** `btnConnect` ile COM portu üzerinden cihaza bağlanma.
* **Zaman Senkronizasyonu:** `btnSyncTime` ile bilgisayarın güncel saatini tek tıkla cihaza yükleme.
* **Mesaj Gönderme:** `btnUpdateDisplay` ile LCD ekrana özel metin yazdırma.
* **Canlı İzleme:** Cihazdan gelen verilerin (Saat, ADC, Buton) anlık olarak Textbox'larda görüntülenmesi.

---

## 🔌 Donanım Bağlantı Şeması (Pin Map)

Kod içerisinde tanımlanan pin konfigürasyonu aşağıdaki gibidir:

| Modül | Tiva Pini | Bağlantı Yeri | Açıklama |
| :--- | :--- | :--- | :--- |
| **UART (PC)** | **PA0** | USB (RX) | Bilgisayardan gelen veriyi alır |
| **UART (PC)** | **PA1** | USB (TX) | Bilgisayara veri gönderir |
| **LCD Kontrol** | **PB0** | LCD RS | Register Select |
| **LCD Kontrol** | **PB1** | LCD E | Enable Pini |
| **LCD Veri** | **PB4 - PB7** | LCD D4-D7 | 4-Bit Veri Yolu |
| **ADC Giriş** | **PE3** | Sensör/Pot | Analog Giriş (AIN0) |
| **Buton** | **PF4** | Dahili SW1 | Pull-Up Dirençli Giriş |

> **Not:** LCD'nin RW bacağı toprağa (GND) bağlanmalıdır.

---

## 📡 Haberleşme Protokolü

Cihaz ve Bilgisayar arasındaki iletişim dili (Protocol) şu şekildedir:
* **Baud Rate:** 9600
* **Data Bits:** 8, **Parity:** None, **Stop Bit:** 1

### 📤 Tiva -> PC (Veri Akışı)
Tiva kartı her saniye şu formatta bir string gönderir:
```text
SS:DD:sn;ADC_DEGERI;BUTON_DURUMU
Ornek: 14:30:05;2048;1

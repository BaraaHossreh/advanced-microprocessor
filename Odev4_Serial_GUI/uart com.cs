using System;
using System.Drawing;
using System.Windows.Forms;
using System.IO.Ports;

namespace MicrocontrollerProject
{
    public partial class MainForm : Form
    {
        public MainForm()
        {
            InitializeComponent();
            // Serial Port varsayılan ayarları (Tiva C ile uyumlu)
            serialPort1.BaudRate = 9600; 
            serialPort1.DataBits = 8;
            serialPort1.StopBits = StopBits.One;
            serialPort1.Parity = Parity.None;
        }

        // 1. BAĞLANTI BUTONU (btnConnect -> Click Olayına Bağla)
        void BtnConnectClick(object sender, EventArgs e)
        {
            try {
                if (!serialPort1.IsOpen) {
                    serialPort1.PortName = txtPort.Text; // Örn: COM3
                    serialPort1.Open();
                    btnConnect.Text = "Stop";
                    btnConnect.BackColor = Color.LightGreen; // Görsel ipucu
                } else {
                    serialPort1.Close();
                    btnConnect.Text = "Start";
                    btnConnect.BackColor = Color.LightGray;
                }
            } catch { 
                MessageBox.Show("Port Hatası! COM numarasını kontrol edin."); 
            }
        }

        // 2. SAAT GÖNDERME (btnSyncTime -> Click Olayına Bağla)
        void BtnSyncTimeClick(object sender, EventArgs e)
        {
            if (serialPort1.IsOpen) 
            {
                // Tiva C "S" + 8 karakter bekliyor (Örn: S12:30:00)
                serialPort1.Write("S" + txtTimeIn.Text);
            }
        }

        // 3. MESAJ GÖNDERME (Mesaj Butonu -> Click Olayına Bağla)
        // Not: Butonun ismini btnUpdateDisplay olarak ayarladığınızdan emin olun.
        void BtnUpdateDisplayClick(object sender, EventArgs e)
{
    if (serialPort1.IsOpen) 
    {
        string msg = txtMsgIn.Text;

        // Force exactly 3 characters (e.g. "A" becomes "A  ")
        if (msg.Length > 3) msg = msg.Substring(0, 3);
        while(msg.Length < 3) msg += " "; 

        // Send 'M' + the 3 characters
        serialPort1.Write("M" + msg);
    }
    else 
    {
        MessageBox.Show("Please connect first!");
    }
}

        // 4. VERİ ALMA (serialPort1 -> DataReceived Olayına Bağla)
        void SerialPort1DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try 
            {
                // Gelen veri formatı: "12:00:00;3.14;0"
                string data = serialPort1.ReadLine(); 
                string[] parts = data.Split(';'); 

                if (parts.Length == 3) {
                    // Arayüzü güncellemek için Invoke (Zorunlu)
                    this.Invoke(new MethodInvoker(delegate {
                        txtTimeOut.Text = parts[0];     // Saat
                        txtAdcOut.Text = parts[1];      // Voltaj
                        
                        // Buton "0" ise Basıldı, "1" ise Bırakıldı (Tiva C Pull-up mantığı)
                        txtStatus.Text = (parts[2].Trim() == "1") ? "Pressed" : "Released";
                    }));
                }
            }
            catch { /* Hata olursa program çökmesin diye boş bırakıldı */ }
        }
    }
}
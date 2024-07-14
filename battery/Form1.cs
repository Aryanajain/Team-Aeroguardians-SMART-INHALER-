using Microsoft.VisualBasic.ApplicationServices;
using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection.Emit;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Xml.Linq;
using TheArtOfDev.HtmlRenderer.Adapters;
using uPLibrary.Networking.M2Mqtt;
using uPLibrary.Networking.M2Mqtt.Messages;
using static System.Net.Mime.MediaTypeNames;
using static System.Windows.Forms.AxHost;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;
using static System.Windows.Forms.VisualStyles.VisualStyleElement.ToolBar;
using static Telerik.WinControls.UI.ValueMapper;
using static Telerik.WinControls.VistaAeroTheme.FlyOut;

namespace battery
{
    public partial class Form1 : Form
    {
        private int batteryLevel = 0;
        bool sos_state = false;

        MqttClient client = null;
        string clientId;
        string mqttserverip = "broker.hivemq.com";
        string pub_topic = "inhaler_monitor_callback";
        string sub_topic = "inhaler_monitor";
        double latitude = 0.0;
        double longitude = 0.0;

        public Form1()
        {
            InitializeComponent();

            // Initialize MQTT client
            client = new MqttClient(mqttserverip);
            clientId = Guid.NewGuid().ToString();

            // Subscribe to topics and handle received messages
            client.MqttMsgPublishReceived += Client_MqttMsgPublishReceived;
            client.ConnectionClosed += Client_ConnectionClosed;

            // Connect to the broker
            try
            {
                client.Connect(clientId);

                client.Subscribe(new string[] { sub_topic }, new byte[] { MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE });

                button3.BackColor = Color.LightGreen;
            }
            catch (Exception ex)
            {
                MessageBox.Show($"Error connecting to MQTT broker: {ex.Message}");
                button3.BackColor = Color.Red;
            }

        }


        // Event handler for button click to update battery level
        private async void button1_Click(object sender, EventArgs e)
        {

            //for (int i = 0; i <= 100; i++)
            //{
            //    UpdateBatteryLevel(i);
            //    label1.Text = i.ToString()+"%";
            //    label2.Text = "Cylinder Level :" + i.ToString()+"ml";
            //    await Task.Delay(10); 
            //    Console.WriteLine($": {i}");
            //}

            // Data log viewer Form2
            Form2 form2 = new Form2();
            form2.ShowDialog();
            Console.WriteLine("displaying form2");
        }

        private void UpdateBatteryLevel(int newLevel)
        {
            batteryLevel = newLevel;
            Console.WriteLine($"updated battery level : {batteryLevel}");
        }

     



        private void button2_Click(object sender, EventArgs e)
        {
            sos_state = !sos_state;
            button2.BackColor = sos_state ?  Color.LightGreen : Color.Gray;

            //publish buzzer on
            if (client.IsConnected)
            {
                string payload = "";

                payload = sos_state ? "on" : "off";


                //for inhaler buzzer
                client.Publish(pub_topic, Encoding.UTF8.GetBytes(payload), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, false);
                Console.WriteLine("Message published successfully.");

                //for mobile notification
                //string body_data = " ";
                string body = "{\"title\":\"EMERGENCY\", \"body\":\"" + "The Person is in low pressure area" + "\"}";
                client.Publish("inhaler_mobile", Encoding.UTF8.GetBytes(body), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, false);
                Console.WriteLine("Message published successfully.");


            }
            else
            {
                MessageBox.Show("Not connected to MQTT broker.");
            }

        }




        private void Client_MqttMsgPublishReceived(object sender, MqttMsgPublishEventArgs e)
        {
            // Handle incoming MQTT messages
            string ReceivedMessage = Encoding.UTF8.GetString(e.Message);

            Console.WriteLine($"-----------------------------------------");
            //MessageBox.Show($"Received message: {ReceivedMessage}");
            Console.WriteLine($"Received message: {ReceivedMessage}");



            String data = ReceivedMessage.Substring(1, ReceivedMessage.Length - 2);
            Console.WriteLine($"data string without bracketes : {data}");



            string[] values = data.Split(',');

            Console.WriteLine($"spo2      = {values[0]}");
            Console.WriteLine($"cyl_level = {values[1]}");
            Console.WriteLine($"gps.lat   = {values[2]}");
            Console.WriteLine($"gps.lon   = {values[3]}");

            latitude = double.Parse(values[2]);    
            longitude = double.Parse(values[3]);

            //update cylinder level
            int cyl_level_int = int.Parse(values[1]);

            if(cyl_level_int>200){
                panel2.BackColor = Color.Green;
                Console.WriteLine("cylinder level > 200");

            }else if(cyl_level_int>25 && cyl_level_int < 200){
                panel2.BackColor = Color.Orange;
                Console.WriteLine("cylinder level > 20 && cylinder level < 85");
            }

            else if (cyl_level_int<25){
                panel2.BackColor = Color.Red;
                Console.WriteLine("cylinder level < 20");
            }


            //update cylinder level
            if (label1.InvokeRequired)
            {
                label1.Invoke((MethodInvoker)(() => { label1.Text = values[0] + "%"; }));
            }
            else
            {
                label1.Text = values[0] + "%";
            }

            string label2data = "Cylinder Level: "+ values[1]+"ml";
            if (label2.InvokeRequired)
            {
                label2.Invoke((MethodInvoker)(() => { label2.Text = label2data; }));
            }
            else
            {
                label2.Text = label2data;
            }




            //display map;
            if (label2.InvokeRequired)
            {
                textBox1.Invoke((MethodInvoker)(() => { textBox1.Text = latitude.ToString()+ ","+longitude.ToString(); }));
            }
            else
            {
                textBox1.Text = values[2] + "," + values[3];
            }

            //write the data to a csv file
            Console.WriteLine($"--------------------/---------------------");

            //string filePath = @"C:\inhaler.csv";
            string filePath = @"C:\Users\aryan\Documents\inhaler.csv";


            // Write data to CSV
            WriteToCsv(filePath, data);

            Console.WriteLine("Data has been written to CSV file.");



        }


        private void Client_ConnectionClosed(object sender, EventArgs e)
        {
            // Handle MQTT connection closed
            MessageBox.Show("MQTT connection closed.");
            button3.BackColor = Color.Red;
        }

        private void button3_Click(object sender, EventArgs e)
        {

        }

        private void pictureBox1_Click(object sender, EventArgs e)
        {

        }

        private void label2_Click(object sender, EventArgs e)
        {

        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void button4_Click(object sender, EventArgs e)
        {
            Console.WriteLine("gps data in text : " + textBox1.Text);

            string url = $"https://www.google.com/maps?q={latitude},{longitude}";

            Process.Start(url);//open browser
        }


        private static void WriteToCsv(string filePath, string data)
        {
            // Check if the file exists; create it if it doesn't
            bool fileExists = File.Exists(filePath);
            using (StreamWriter writer = new StreamWriter(filePath, append: true))
            {
                // If the file doesn't exist, write headers 
                if (!fileExists)
                {
                    //write header
                    writer.WriteLine("");
                }



                //current date and time
                DateTime currentDateTime = DateTime.Now;
                Console.WriteLine($"Current Date and Time: {currentDateTime}");

                int year = currentDateTime.Year;
                int month = currentDateTime.Month;
                int day = currentDateTime.Day;
                int hour = currentDateTime.Hour;
                int minute = currentDateTime.Minute;
                int second = currentDateTime.Second;


                Console.WriteLine($"Year: {year}, Month: {month}, Day: {day}");
                Console.WriteLine($"Hour: {hour}, Minute: {minute}, Second: {second}");

                // Format the date and time as needed
                string formattedDateTime = currentDateTime.ToString("yyyy-MM-dd HH:mm:ss");
                Console.WriteLine($"Formatted Date and Time: {formattedDateTime}");

                Console.WriteLine(formattedDateTime + " => " + data + "#");

                writer.WriteLine(formattedDateTime + " => " + data + "#");

            }
        }


    }
}

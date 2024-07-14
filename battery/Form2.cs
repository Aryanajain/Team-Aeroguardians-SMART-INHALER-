using System;
using System.IO;
using System.Threading.Tasks;
using System.Windows.Forms;
using Microsoft.VisualBasic.FileIO;

namespace battery
{
    public partial class Form2 : Form
    {
        //string csvFilePath = @"C:\inhaler.csv"; // CSV file path
        string csvFilePath = @"C:\Users\aryan\Documents\inhaler.csv";
        public Form2()
        {
            InitializeComponent();
            listView1.View = View.Details;
            listView1.Columns.Add("Data Logs");
            listView1.Columns[0].Width = 450;

            // Check if the file exists
            if (!File.Exists(csvFilePath))
            {
                // Create the file and write default content (if needed)
                using (var sw = File.CreateText(csvFilePath))
                {
                    sw.WriteLine("Default,Content,Here"); // Example default content
                }
            }
        }

        private void button3_Click(object sender, EventArgs e)
        {
            try
            {
                this.Close();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Error occurred: " + ex.Message);
            }
        }

        private void button2_Click(object sender, EventArgs e)
        {
            listView1.Items.Clear();
        }

        private async void button1_Click(object sender, EventArgs e)
        {


            // read CSV file line by line
            using (TextFieldParser parser = new TextFieldParser(csvFilePath))
            {
                parser.TextFieldType = FieldType.Delimited;
                parser.SetDelimiters("#");

                while (!parser.EndOfData)
                {
                    string[] fields = parser.ReadFields();

                    listView1.Items.Add(new ListViewItem(fields));
                }
            }

            //try
            //{
            //    // Clear the ListView before reading the file
            //    listView1.Items.Clear();

            //    // Read CSV file line by line asynchronously
            //    await Task.Run(() =>
            //    {
            //        using (TextFieldParser parser = new TextFieldParser(csvFilePath))
            //        {
            //            parser.TextFieldType = FieldType.Delimited;
            //            parser.SetDelimiters(",");

            //            // Check if there is data
            //            bool hasData = false;

            //            while (!parser.EndOfData)
            //            {
            //                string[] fields = parser.ReadFields();
            //                hasData = true; // Set flag if we read any fields

            //                // Invoke on UI thread to add items to the ListView
            //                listView1.Invoke(new Action(() =>
            //                {
            //                    listView1.Items.Add(new ListViewItem(fields));
            //                }));
            //            }

            //            // If no data was found, you can handle any additional logic here if needed
            //            if (!hasData)
            //            {
            //                // Optional: Show a message if desired
            //                listView1.Invoke(new Action(() =>
            //                {
            //                    MessageBox.Show("No data found in the CSV file.");
            //                }));
            //            }
            //        }
            //    });
            //}
            //catch (Exception ex)
            //{
            //    MessageBox.Show("Error reading the file: " + ex.Message);
            //}
        }

    }
}

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Text.RegularExpressions;

namespace Intel.RealSense
{
    class ADP_TestLoadSettingsJson
    {

        public bool LoadSettingsJson(Device device)
        {
            var deviceName = device.Info[CameraInfo.Name];
            var usbType = device.Info[CameraInfo.UsbTypeDescriptor];
            string[] deviceNameParts = deviceName.Split(" ".ToCharArray());
            var model = deviceNameParts.Last().Trim();


            SerializableDevice ser = null;
            Console.WriteLine($"Device model: '{model}'");
            
            var jsonFileName = $"ADP_{model}_TEST_JSON_USB{usbType}.json";
            var jsonPath = System.IO.Path.Combine(System.IO.Directory.GetCurrentDirectory(), "JsonConfigs", jsonFileName);
            Console.WriteLine($"Searching for a json test file: '{jsonFileName}'");
            if (!File.Exists(jsonPath))
            {
                Console.WriteLine($"File not found, so no settings data will be loaded into a camera. Exiting...");
                return false;
            }

            Console.WriteLine($"Json test file found: {jsonPath}. Loading it...");
            Console.WriteLine($"Loading by using of SerialiazableDevice...");
            ser = SerializableDevice.FromDevice(device);
            try
            {
                ser.JsonConfiguration = File.ReadAllText(jsonPath);
                Console.WriteLine($"Loaded");
                Console.WriteLine(ser.JsonConfiguration);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"Failed to load json file: {ex.Message}");
                return false;
            }

            // check for manual options setting
            var depthSensor = device.FirstDepthSensor();
            var colorSensor = device.FirstColorSensor();
            Console.WriteLine($"\nManulally read options:");
            Console.WriteLine($"--------------------------------------------------");            
            Console.WriteLine($"DEPTH OPTIONS:");
            Console.WriteLine($"================");
            Console.WriteLine($"Total count: {depthSensor.Options.Count()}\n");
            for (int i = 0; i < depthSensor.Options.Count(); i++)
            {
                // there may be some exceptions when trying to get some options, even when going through by index. So using this relatively safe method to iterate all the options, even unacceptible
                try
                {
                    var opt = depthSensor.Options.Skip(i).Take(1).FirstOrDefault();
                    if (opt != null)
                    {
                        Console.WriteLine($"{opt.Key}: {opt.Value}");
                        
                    }
                }
                catch (Exception)
                {
                    Console.WriteLine($"Error getting option №{i}");
                }                
            }
            Console.WriteLine($"\nCOLOR OPTIONS:");
            Console.WriteLine($"================");
            Console.WriteLine($"Total count: {colorSensor.Options.Count()}\n");
            for (int i = 0; i < colorSensor.Options.Count(); i++)
            {
                // there may be some exceptions when trying to get some options, even when going through by index. So using this relatively safe method to iterate all the options, even unacceptible
                try
                {
                    var opt = colorSensor.Options.Skip(i).Take(1).FirstOrDefault();
                    if (opt != null)
                    {
                        Console.WriteLine($"{opt.Key}: {opt.Value}");
                        if (opt.Key.ToString() == "Brightness") // check if we can set value to option. We take brightness for example
                        {
                            var newValue = opt.Value + 1;
                            Console.Write($"\nCheck if we can set option manually: {opt.Key}: from {opt.Value} to {newValue}...");                            
                            opt.Value = newValue;
                            
                            var optionsJson = Regex.Replace(ser.JsonConfiguration, @" ", "");
                            // if we can read brightness from json settings (not all cameras returns all params), use it
                            if (optionsJson.IndexOf("controls-color-brightness") > 0)
                            {
                                var shouldBe = $"\"controls-color-brightness\":\"{newValue}\"";
                                Console.Write($"new value set: {optionsJson.IndexOf(shouldBe) > 0}\n\n");
                            }
                            else // else just reread value from option - it's getter will get it from device
                            {
                                Console.Write($"new value set: {opt.Value == newValue}\n\n"); // opt.Value getter will read value from device
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"Error read option №{i}: {ex.Message}");
                }
            }

            return true;
        }
    }

}

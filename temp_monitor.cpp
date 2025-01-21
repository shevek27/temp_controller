#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>


namespace fs = std::filesystem;

// read temperature form sysfs

float read_cpu_temp(const std::string& filepath)
{
    std::ifstream temp_file(filepath);
    if (!temp_file.is_open())
    {
        std::cerr << "error! cant open temp file at " << filepath << std::endl;
        return -1;
    }

    int mili_degrees;
    temp_file >> mili_degrees;
    temp_file.close();

    int degrees = mili_degrees / 1000.0;
    return degrees;
}

std::vector<std::string> find_hwmon_temperature_files()
{
    std::vector<std::string> temp_files;

    for (const auto& entry : fs::directory_iterator("/sys/class/hwmon/"))
    {
        for (const auto& file : fs::directory_iterator(entry.path()))
        {
            // npos = no position
            if (file.path().filename().string().find("temp") != std::string::npos &&
            file.path().filename().string().find("_input") != std::string::npos)
            {
                temp_files.push_back(file.path().string());
            }
        }
    }
    
    return temp_files;
}

std::string find_cpu_frequency_control_file()
{
    // specifically cpu0 but controls frequency for all cpu cores
    std::string frequency_file = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq";
    if (fs::exists(frequency_file))
    {
        return frequency_file;
    }
    else
    {
        std::cerr << "error: no cpu frequency control file found." << std::endl;
        return "";
    }

}

void set_cpu_frequency(const std::string& frequency_path, const std::string& frequency)
{
    std::ofstream frequency_file(frequency_path);
    if (!frequency_file.is_open())
    {
        std::cerr << "ERROR: unable to open cpu frequency file at " << frequency_path << std::endl;
        return;
    }

    else
    {
        frequency_file << frequency;
        frequency_file.close();
    }
}


int main()
{
    std::vector<std::string> temp_files = find_hwmon_temperature_files();

    if (temp_files.empty())
    {
        std::cerr << "error: no cpu temperature sensors found, problem with lm-sensors?" << std::endl;
        return 1;
    }

    std::string cpu_frequency_file = find_cpu_frequency_control_file();
    if (cpu_frequency_file.empty())
    {
        return 1;
    }


    while (true)
    {
        for (const auto& temp_file : temp_files)
        {
            float temperature = read_cpu_temp(temp_file);
            if (temperature >= 0)
            {
                std::cout << "sensor: " << temp_file << " | temperature: " << temperature << "°C" << std::endl;

                // adjust cpu frequency based on temperature
                std::string max_frequency;
                if (temperature < 50)
                {
                    max_frequency = "350000";                   
                }
                else if (temperature < 70)
                {
                    max_frequency = "180000";
                }
                else
                {
                    max_frequency = "120000";
                }

                set_cpu_frequency(cpu_frequency_file, max_frequency);
                std::cout << "set cpu frequency to: " << max_frequency << std::endl;
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return 0;

}
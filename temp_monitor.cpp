#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <chrono>
#include <signal.h>

namespace fs = std::filesystem;
std::string DEFAULT_MAX_FREQUENCY;
std::string ORIGINAL_GOVERNOR;

// some processors have different standard "settable" frequencies. These will work as guidelines.
std::string high_frequency = "3500000";
std::string mid_frequency = "2500000";
std::string low_frequency = "1200000";


bool is_root()
{
    return getuid() == 0;
}


// governors stuff

std::string get_current_governor()
{
    std::ifstream governor_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
    std::string governor;
    if (governor_file.is_open())
    {
        governor_file >> governor;
        governor_file.close();
    }
    return governor;
}


void set_cpu_governor(const std::string& governor)
{
    // have to restore for every cpu core
    for (const auto& entry : fs::directory_iterator("/sys/devices/system/cpu"))
    {
        if (fs::is_directory(entry))
        {
            std::ofstream governor_file(entry.path().string() + "/cpufreq/scaling_governor");
            if (governor_file.is_open())
            {
                governor_file << governor;
                governor_file.close();
                std::cout << "cpu governor set to: " << governor << " for " << entry.path() << std::endl;
            }
            else
            {
                ;
                //std::cerr << "error! unable to set governor for " << entry.path() << std::endl;
            }
        }
    }
}

void restore_cpu_governor()
{
    // have to restore for every cpu core
    for (const auto& entry : fs::directory_iterator("/sys/devices/system/cpu"))
    {
        if (fs::is_directory(entry))
        {
            std::ofstream governor_file(entry.path().string() + "/cpufreq/scaling_governor");
            if (governor_file.is_open())
            {
                governor_file << ORIGINAL_GOVERNOR;
                governor_file.close();
                std::cout << "cpu governor set to: " << ORIGINAL_GOVERNOR << " for " << entry.path() << std::endl;
            }
            else
            {
                ;
                //std::cerr << "error! unable to set governor for " << entry.path() << std::endl;
            }
        }
    }
}


// read temperature in from a hwmon system file and return it in celcius
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

    // hwmon is where the temp files are, they are named hwmon0, hwmon1, etc.
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

// write the frequency to the scaling_max_freq file
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

std::string get_current_frequency(const std::string& frequency_path)
{
    std::ifstream frequency_file(frequency_path);
    if (frequency_file.is_open() == false)
    {
        std::cerr << "error, unable to get current frequency from " << frequency_path << std::endl;
        return "";
    }
    else
    {
        std::string current_frequency;
        frequency_file >> current_frequency;
        return current_frequency; 
    }
}

void restore_default_frequency()
{
    if (DEFAULT_MAX_FREQUENCY.empty() == false)
    {
        std::string cpu_freq_file = find_cpu_frequency_control_file();
        if (cpu_freq_file.empty() == false)
        {
            set_cpu_frequency(cpu_freq_file, DEFAULT_MAX_FREQUENCY);
            std::cout << "restores default max frequency: " << DEFAULT_MAX_FREQUENCY << std::endl;
        }
    }
   
}

void cleanup(int signal)
{
    restore_default_frequency();
    restore_cpu_governor();
    exit(signal);
}




int main()
{
    if (is_root() == false)
    {
        std::cerr << "error! must run as root, use sudo." << std::endl;
        return 1;
    }

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

    std::cout << cpu_frequency_file << std::endl;
    DEFAULT_MAX_FREQUENCY = get_current_frequency(cpu_frequency_file);
    ORIGINAL_GOVERNOR = get_current_governor();
    // these are to change the max frequency back when the program ends!
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    //set_cpu_governor("userspace");

    while (true)
    {
        float current_max_temp = -1.0;
        for (const auto& temp_file : temp_files)
        {
            float temperature = read_cpu_temp(temp_file);
            if (temperature > current_max_temp)
            {
                current_max_temp = temperature;
            }
        }
        float real_temp = current_max_temp;
        if (real_temp >= 0)
            {
                std::cout << "temperature: " << real_temp << "°C" << std::endl;

                // adjust cpu frequency based on temperature
                std::string max_frequency;
                if (real_temp <= 50)
                {
                    max_frequency = high_frequency;
                }
                else if (50 < real_temp && real_temp < 70)
                {
                    max_frequency = mid_frequency;
                }
                else
                {
                    max_frequency = low_frequency;
                }

                set_cpu_frequency(cpu_frequency_file, max_frequency);
                std::string real_frequency = get_current_frequency(cpu_frequency_file);
                float current_ghz = std::stof (real_frequency) / 1000;
                std::cout << "set cpu frequency to: " << current_ghz <<  "MHz" << std::endl;
            }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    return 0;

}